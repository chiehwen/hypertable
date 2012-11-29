/** -*- c++ -*-
 * Copyright (C) 2007-2012 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 3 of the
 * License, or any later version.
 *
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "Common/Compat.h"
#include "Common/Error.h"
#include "Common/md5.h"
#include "Common/FailureInducer.h"
#include "Common/ScopeGuard.h"

#include "OperationRecover.h"
#include "OperationRecoverRanges.h"
#include "Hypertable/Lib/MetaLogReader.h"
#include "Hypertable/Lib/MetaLogEntityRange.h"
#include "Hypertable/RangeServer/MetaLogDefinitionRangeServer.h"
#include "BalancePlanAuthority.h"

using namespace Hypertable;
using namespace Hyperspace;


OperationRecover::OperationRecover(ContextPtr &context, 
        RangeServerConnectionPtr &rsc)
  : Operation(context, MetaLog::EntityType::OPERATION_RECOVER_SERVER),
    m_location(rsc->location()), m_rsc(rsc), m_hyperspace_handle(0), 
    m_lock_acquired(false) {
  m_subop_dependency = format("operation-id-%lld", (Lld)id());
  m_dependencies.insert(m_subop_dependency);
  m_dependencies.insert(Dependency::RECOVERY_BLOCKER);
  m_dependencies.insert(Dependency::RECOVERY);
  m_exclusivities.insert(m_rsc->location());
  m_obstructions.insert(Dependency::RECOVER_SERVER);
  m_hash_code = md5_hash("RecoverServer") ^ md5_hash(m_rsc->location().c_str());
  HT_ASSERT(m_rsc != 0);
  m_rsc->set_recovering(true);
}

OperationRecover::OperationRecover(ContextPtr &context,
    const MetaLog::EntityHeader &header_)
  : Operation(context, header_), m_hyperspace_handle(0), m_lock_acquired(false) {
  m_subop_dependency = format("operation-id-%lld", (Lld)id());
}


void OperationRecover::execute() {
  std::vector<Entity *> entities;
  Operation *sub_op;
  int state = get_state();
  String subject, message;

  HT_INFOF("Entering RecoverServer %s state=%s this=%p",
           m_location.c_str(), OperationState::get_text(state), (void *)this);
  if (!m_rsc)
    (void)m_context->rsc_manager->find_server_by_location(m_location, m_rsc);
  else
    HT_ASSERT(m_location == m_rsc->location());

  if (m_hostname.empty() && m_rsc)
    m_hostname = m_rsc->hostname();

  if (!acquire_server_lock()) {
    if (m_rsc)
      m_rsc->set_recovering(false);
    complete_ok();
    return;
  }

  switch (state) {
  case OperationState::INITIAL:

    // Send notification
    subject = format("NOTICE: Recovery of %s (%s) starting",
                     m_location.c_str(), m_hostname.c_str());
    message = format("Failure of range server %s (%s) has been detected.  "
                     "Starting recovery...", m_location.c_str(),
                     m_hostname.c_str());
    HT_INFO_OUT << message << HT_END;
    m_context->notification_hook(subject, message);

    // read rsml figure out what types of ranges lived on this server
    // and populate the various vectors of ranges
    read_rsml();

    // now create a new recovery plan
    create_recovery_plan();

    set_state(OperationState::ISSUE_REQUESTS);

    HT_MAYBE_FAIL("recover-server-1");
    m_context->mml_writer->record_state(this);
    HT_MAYBE_FAIL("recover-server-2");
    break;

  case OperationState::ISSUE_REQUESTS:
    HT_ASSERT(!m_subop_dependency.empty());
    if (m_root_specs.size()) {
      sub_op = new OperationRecoverRanges(m_context, m_location,
                                          RangeSpec::ROOT);
      HT_INFO_OUT << "Number of root ranges to recover for location " 
          << m_location << "="
          << m_root_specs.size() << HT_END;
      sub_op->add_obstruction(m_subop_dependency);
      m_sub_ops.push_back(sub_op);
      entities.push_back(sub_op);
    }
    if (m_metadata_specs.size()) {
      sub_op = new OperationRecoverRanges(m_context, m_location,
                                          RangeSpec::METADATA);
      HT_INFO_OUT << "Number of metadata ranges to recover for location "
          << m_location << "="
          << m_metadata_specs.size() << HT_END;
      sub_op->add_obstruction(m_subop_dependency);
      m_sub_ops.push_back(sub_op);
      entities.push_back(sub_op);
    }
    if (m_system_specs.size()) {
      sub_op = new OperationRecoverRanges(m_context, m_location,
                                          RangeSpec::SYSTEM);
      HT_INFO_OUT << "Number of system ranges to recover for location "
          << m_location << "="
          << m_system_specs.size() << HT_END;
      sub_op->add_obstruction(m_subop_dependency);
      m_sub_ops.push_back(sub_op);
      entities.push_back(sub_op);
    }
    if (m_user_specs.size()) {
      sub_op = new OperationRecoverRanges(m_context, m_location,
                                          RangeSpec::USER);
      HT_INFO_OUT << "Number of user ranges to recover for location " 
          << m_location << "="
          << m_user_specs.size() << HT_END;
      sub_op->add_obstruction(m_subop_dependency);
      m_sub_ops.push_back(sub_op);
      entities.push_back(sub_op);
    }
    set_state(OperationState::FINALIZE);
    entities.push_back(this);
    m_context->mml_writer->record_state(entities);
    HT_MAYBE_FAIL("recover-server-3");
    break;

  case OperationState::FINALIZE:
    // Once recovery is complete, the master blows away the RSML and CL for the
    // server being recovered then it unlocks the hyperspace file
    clear_server_state();
    HT_MAYBE_FAIL("recover-server-4");
    complete_ok();
    // Send notification
    subject = format("NOTICE: Recovery of %s (%s) succeeded",
                     m_location.c_str(), m_hostname.c_str());
    message = format("Recovery of range server %s (%s) has succeeded.",
                     m_location.c_str(), m_hostname.c_str());
    m_context->notification_hook(subject, message);
    break;

  default:
    HT_FATALF("Unrecognized state %d", state);
    break;
  }

  HT_INFOF("Leaving RecoverServer %s state=%s this=%p",
           m_location.c_str(), OperationState::get_text(get_state()), 
           (void *)this);
}

OperationRecover::~OperationRecover() {
}


bool OperationRecover::acquire_server_lock() {
  String subject, message;

  if (m_lock_acquired)
    return true;

  try {
    String fname = m_context->toplevel_dir + "/servers/" + m_location;
    uint32_t oflags = OPEN_FLAG_READ | OPEN_FLAG_WRITE | OPEN_FLAG_LOCK;
    uint32_t lock_status = LOCK_STATUS_BUSY;
    LockSequencer sequencer;
    uint64_t handle = 0;
    
    HT_ON_SCOPE_EXIT(&Hyperspace::close_handle_ptr, m_context->hyperspace, &handle);

    handle = m_context->hyperspace->open(fname, oflags);

    m_context->hyperspace->try_lock(handle, 
                                    LOCK_MODE_EXCLUSIVE, &lock_status,
                                    &sequencer);
    if (lock_status != LOCK_STATUS_GRANTED) {
      HT_INFO_OUT << "Couldn't obtain lock on '" << fname
                  << "' due to conflict, lock_status=" << lock_status << HT_END;
      // Send notification
      subject = format("NOTICE: Recovery of %s (%s) aborted",
                       m_location.c_str(), m_hostname.c_str());
      message = format("Aborting recovery of range server %s (%s) because "
                       "unable to aquire lock.", m_location.c_str(),
                       m_hostname.c_str());
      HT_INFO_OUT << message << HT_END;
      m_context->notification_hook(subject, message);
      return false;
    }

    m_context->hyperspace->attr_set(handle, "removed", "", 0);

    m_hyperspace_handle = handle;
    handle = 0;
    m_lock_acquired = true;

    HT_INFO_OUT << "Acquired lock on '" << fname 
                << "', starting recovery..." << HT_END;
  }
  catch (Exception &e) {
    HT_ERROR_OUT << "Problem obtaining " << m_location 
                 << " hyperspace lock (" << e << "), aborting..." << HT_END;
    // Send notification
    subject = format("ERROR: Recovery of %s (%s) failed",
                     m_location.c_str(), m_hostname.c_str());
    message 
      = format("Attempt to aquire lock on range server %s (%s) failed.\\n\\n"
               "  %s - %s\\n", m_location.c_str(), m_hostname.c_str(),
               Error::get_text(e.code()), e.what());
    HT_ERROR_OUT << subject << ": " << e << HT_END;
    m_context->notification_hook(subject, message);
    return false;
  }
  return true;
}

void OperationRecover::display_state(std::ostream &os) {
  os << " location=" << m_location << " ";
}

const String OperationRecover::name() {
  return label();
}

const String OperationRecover::label() {
  return format("RecoverServer %s", m_location.c_str());
}

void OperationRecover::clear_server_state() {
  // remove this RangeServerConnection entry
  //
  // if m_rsc is NULL then it was already removed
  if (m_rsc) {
    HT_INFO_OUT << "delete RangeServerConnection from mml for "
        << m_location << HT_END;
    m_context->mml_writer->record_removal(m_rsc.get());
    m_context->rsc_manager->erase_server(m_rsc);

    // drop server from monitor list
    m_context->monitoring->drop_server(m_rsc->location());
    HT_MAYBE_FAIL("recover-server-4");
  }
  // unlock hyperspace file
  Hyperspace::close_handle_ptr(m_context->hyperspace, &m_hyperspace_handle);
  // delete balance plan
  BalancePlanAuthority *plan = m_context->get_balance_plan_authority();
  plan->remove_recovery_plan(m_location);
}

void OperationRecover::create_recovery_plan() {
  BalancePlanAuthority *plan = m_context->get_balance_plan_authority();
  plan->create_recovery_plan(m_location,
                             m_root_specs, m_root_states,
                             m_metadata_specs, m_metadata_states,
                             m_system_specs, m_system_states,
                             m_user_specs, m_user_states);
}

void OperationRecover::read_rsml() {
  // move rsml and commit log to some recovered dir
  MetaLog::DefinitionPtr rsml_definition
      = new MetaLog::DefinitionRangeServer(m_location.c_str());
  MetaLog::ReaderPtr rsml_reader;
  MetaLog::EntityRange *range;
  vector<MetaLog::EntityPtr> entities;
  String logfile;

  try {
    logfile = m_context->toplevel_dir + "/servers/" + m_location + "/log/"
        + rsml_definition->name();
    rsml_reader = new MetaLog::Reader(m_context->dfs, rsml_definition, logfile);
    rsml_reader->get_entities(entities);
    foreach_ht (MetaLog::EntityPtr &entity, entities) {
      if ((range = dynamic_cast<MetaLog::EntityRange *>(entity.get())) != 0) {
        QualifiedRangeSpec spec;
        // skip phantom ranges, let whoever was recovering them deal with them
        if (!(range->state.state & RangeState::PHANTOM)) {
          HT_INFO_OUT << "Range " << *range << ": not PHANTOM; including" << HT_END;
          spec.table = range->table;
          spec.range = range->spec;
          if (spec.is_root()) {
            m_root_specs.push_back(QualifiedRangeSpec(m_arena, spec));
            m_root_states.push_back(RangeState(m_arena, range->state));
          }
          else if (spec.table.is_metadata()) {
            m_metadata_specs.push_back(QualifiedRangeSpec(m_arena, spec));
            m_metadata_states.push_back(RangeState(m_arena, range->state));
          }
          else if (spec.table.is_system()) {
            m_system_specs.push_back(QualifiedRangeSpec(m_arena, spec));
            m_system_states.push_back(RangeState(m_arena, range->state));
          }
          else {
            m_user_specs.push_back(QualifiedRangeSpec(m_arena, spec));
            m_user_states.push_back(RangeState(m_arena, range->state));
          }
        }
        else {
          // Cleanup unused transfer log
          m_context->dfs->rmdir(range->state.transfer_log);
        }
      }
    }
  }
  catch (Exception &e) {
    HT_FATAL_OUT << e << HT_END;
  }
}

size_t OperationRecover::encoded_state_length() const {
  size_t len = Serialization::encoded_length_vstr(m_location) + 16;
  for (size_t i=0; i<m_root_specs.size(); i++)
    len += m_root_specs[i].encoded_length() + m_root_states[i].encoded_length();
  for (size_t i=0; i<m_metadata_specs.size(); i++)
    len += m_metadata_specs[i].encoded_length() + m_metadata_states[i].encoded_length();
  for (size_t i=0; i<m_system_specs.size(); i++)
    len += m_system_specs[i].encoded_length() + m_system_states[i].encoded_length();
  for (size_t i=0; i<m_user_specs.size(); i++)
    len += m_user_specs[i].encoded_length() + m_user_states[i].encoded_length();
  return len;
}

void OperationRecover::encode_state(uint8_t **bufp) const {
  Serialization::encode_vstr(bufp, m_location);
  // root
  Serialization::encode_i32(bufp, m_root_specs.size());
  for (size_t i=0; i<m_root_specs.size(); i++) {
    m_root_specs[i].encode(bufp);
    m_root_states[i].encode(bufp);
  }
  // metadata
  Serialization::encode_i32(bufp, m_metadata_specs.size());
  for (size_t i=0; i<m_metadata_specs.size(); i++) {
    m_metadata_specs[i].encode(bufp);
    m_metadata_states[i].encode(bufp);
  }
  // system
  Serialization::encode_i32(bufp, m_system_specs.size());
  for (size_t i=0; i<m_system_specs.size(); i++) {
    m_system_specs[i].encode(bufp);
    m_system_states[i].encode(bufp);
  }
  // user
  Serialization::encode_i32(bufp, m_user_specs.size());
  for (size_t i=0; i<m_user_specs.size(); i++) {
    m_user_specs[i].encode(bufp);
    m_user_states[i].encode(bufp);
  }
}

void OperationRecover::decode_state(const uint8_t **bufp, size_t *remainp) {
  decode_request(bufp, remainp);
}

void OperationRecover::decode_request(const uint8_t **bufp, size_t *remainp) {

  m_location = Serialization::decode_vstr(bufp, remainp);
  int nn;
  QualifiedRangeSpec spec;
  RangeState state;
  nn = Serialization::decode_i32(bufp, remainp);
  for (int ii = 0; ii < nn; ++ii) {
    spec.decode(bufp, remainp);
    m_root_specs.push_back(QualifiedRangeSpec(m_arena, spec));
    state.decode(bufp, remainp);
    m_root_states.push_back(RangeState(m_arena, state));
  }
  nn = Serialization::decode_i32(bufp, remainp);
  for (int ii = 0; ii < nn; ++ii) {
    spec.decode(bufp, remainp);
    m_metadata_specs.push_back(QualifiedRangeSpec(m_arena, spec));
    state.decode(bufp, remainp);
    m_metadata_states.push_back(RangeState(m_arena, state));
  }
  nn = Serialization::decode_i32(bufp, remainp);
  for (int ii = 0; ii < nn; ++ii) {
    spec.decode(bufp, remainp);
    m_system_specs.push_back(QualifiedRangeSpec(m_arena, spec));
    state.decode(bufp, remainp);
    m_system_states.push_back(RangeState(m_arena, state));
  }
  nn = Serialization::decode_i32(bufp, remainp);
  for (int ii = 0; ii < nn; ++ii) {
    spec.decode(bufp, remainp);
    m_user_specs.push_back(QualifiedRangeSpec(m_arena, spec));
    state.decode(bufp, remainp);
    m_user_states.push_back(RangeState(m_arena, state));
  }
}

