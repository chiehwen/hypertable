/*
 * Copyright (C) 2007-2013 Hypertable, Inc.
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

#include <cmath>

#include "Common/Error.h"
#include "Common/FailureInducer.h"
#include "Common/Serialization.h"
#include "Common/StringExt.h"
#include "Common/Time.h"
#include "Common/md5.h"

#include "Hyperspace/Session.h"

#include <boost/algorithm/string.hpp>

#include "RangeServerHyperspaceCallback.h"
#include "LoadBalancer.h"
#include "OperationRegisterServer.h"
#include "OperationProcessor.h"

using namespace Hypertable;

OperationRegisterServer::OperationRegisterServer(ContextPtr &context,
                                                 EventPtr &event)
  : OperationEphemeral(context, event, MetaLog::EntityType::OPERATION_REGISTER_SERVER) {
  const uint8_t *ptr = event->payload;
  size_t remaining = event->payload_len;
  decode_request(&ptr, &remaining);

  if (!m_location.empty()) {
    add_obstruction(String("RegisterServer ") + m_location);
    add_dependency(String("RegisterServerBlocker ") + m_location);
  }

  m_local_addr = InetAddr(event->addr);
  m_public_addr = InetAddr(m_system_stats.net_info.primary_addr, m_listen_port);
  m_received_ts = get_ts64();
}


void OperationRegisterServer::execute() {
  bool newly_created = false;
  std::vector<SystemVariable::Spec> specs;
  uint64_t generation;

  if (m_location == "") {
    if (!m_context->rsc_manager->find_server_by_hostname(m_system_stats.net_info.host_name, m_rsc))
      m_context->rsc_manager->find_server_by_public_addr(m_public_addr, m_rsc);
    if (m_rsc) {
      // If range server has been removed, let it get re-assigned a new location
      if (m_rsc->get_removed())
        m_rsc = 0;
      else
        m_location = m_rsc->location();
    }
  }
  else
    m_context->rsc_manager->find_server_by_location(m_location, m_rsc);

  // Clean up existing connection (happens when connect arrives before
  // disconnect notification from Hyperspace)
  if (m_rsc)
    m_context->rsc_manager->disconnect_server(m_rsc);
  else if (m_location == "") {
    uint64_t id = m_context->hyperspace->attr_incr(m_context->master_file_handle,
                                                   "next_server_id");
    if (m_context->location_hash.empty())
      m_location = format("rs%llu", (Llu)id);
    else
      m_location = format("rs-%s-%llu", m_context->location_hash.c_str(),
                          (Llu)id);
    newly_created = true;

    String fname = m_context->toplevel_dir + "/servers/";
    // !!! wrap in try/catch
    m_context->hyperspace->mkdirs(fname);
    fname += m_location;
    uint32_t oflags = Hyperspace::OPEN_FLAG_READ | Hyperspace::OPEN_FLAG_CREATE;
    // !!! wrap in try/catch
    uint64_t handle = m_context->hyperspace->open(fname, oflags);
    m_context->hyperspace->close(handle);
  }

  // Remove pending OperationRecover operations for this location
  {
    int64_t hash_code= md5_hash("RecoverServer") ^ md5_hash(m_location.c_str());
    OperationPtr operation = m_context->op->remove_operation(hash_code);
    if (operation)
      operation->complete_ok();
  }

  if (!m_rsc)
    m_rsc = new RangeServerConnection(m_location,
                                      m_system_stats.net_info.host_name,
                                      m_public_addr);

  // this connection has been marked for removal
  if (m_rsc->get_removed()) {
    String errstr = format("Detected RangeServer marked removed while"
            "registering server %s(%s), as location %s",
            m_system_stats.net_info.host_name.c_str(),
            m_public_addr.format().c_str(),
            m_location.c_str());
    complete_error(Error::MASTER_RANGESERVER_IN_RECOVERY, errstr);
    HT_ERROR_OUT << errstr << HT_END;

    CommHeader header;
    header.initialize_from_request_header(m_event->header);
    header.command = RangeServerProtocol::COMMAND_INITIALIZE;
    m_context->system_state->get(specs, &generation);
    CommBufPtr cbp(new CommBuf(header, encoded_response_length(generation, specs)));
    encode_response(generation, specs, cbp->get_data_ptr_address());

    int error = m_context->comm->send_response(m_event->addr, cbp);
    if (error != Error::OK)
      HT_ERRORF("Problem sending response (location=%s) back to %s",
              m_location.c_str(), m_event->addr.format().c_str());
    return;
  }

  if (m_rsc->get_handle() == 0) {
    String fname = m_context->toplevel_dir + "/servers/" + m_location;
    RangeServerHyperspaceCallback *rscb
      = new RangeServerHyperspaceCallback(m_context, m_rsc);
    Hyperspace::HandleCallbackPtr cb(rscb);
    uint64_t handle = m_context->hyperspace->open(fname, Hyperspace::OPEN_FLAG_READ|Hyperspace::OPEN_FLAG_CREATE, cb);
    HT_ASSERT(handle);
    m_rsc->set_handle(handle);
  }

  if (!m_context->rsc_manager->connect_server(m_rsc, m_system_stats.net_info.host_name,
                                              m_local_addr, m_public_addr)) {

    complete_error(Error::CONNECT_ERROR_MASTER,
                   format("Problem connecting location %s",m_location.c_str()));
    CommHeader header;
    header.initialize_from_request_header(m_event->header);
    header.command = RangeServerProtocol::COMMAND_INITIALIZE;
    m_context->system_state->get(specs, &generation);
    CommBufPtr cbp(new CommBuf(header, encoded_response_length(generation, specs)));
    encode_response(generation, specs, cbp->get_data_ptr_address());
    int error = m_context->comm->send_response(m_event->addr, cbp);
    if (error != Error::OK)
      HT_ERRORF("Problem sending response (location=%s) back to %s",
                m_location.c_str(), m_event->addr.format().c_str());
    return;
  }

  int32_t difference = (int32_t)abs((m_received_ts - m_register_ts) / 1000LL);
  if (difference > (3000000 + m_context->max_allowable_skew)) {
    String errstr = format("Detected clock skew while registering server "
            "%s (%s), as location %s register_ts=%llu, received_ts=%llu, "
            "difference=%d > allowable skew %d", 
            m_system_stats.net_info.host_name.c_str(), 
            m_public_addr.format().c_str(), m_location.c_str(), 
            (Llu)m_register_ts, (Llu)m_received_ts, difference, 
            m_context->max_allowable_skew);
    String subject = format("Clock skew detected while registering %s (%s)",
                            m_system_stats.net_info.host_name.c_str(),
                            m_location.c_str());
    m_context->notification_hook(subject, errstr);
    complete_error(Error::RANGESERVER_CLOCK_SKEW, errstr);
    HT_ERROR_OUT << errstr << HT_END;
    // clock skew detected by master
    CommHeader header;
    header.initialize_from_request_header(m_event->header);
    header.command = RangeServerProtocol::COMMAND_INITIALIZE;
    m_context->system_state->get(specs, &generation);
    CommBufPtr cbp(new CommBuf(header, encoded_response_length(generation, specs)));

    encode_response(generation, specs, cbp->get_data_ptr_address());
    int error = m_context->comm->send_response(m_event->addr, cbp);
    if (error != Error::OK)
      HT_ERRORF("Problem sending response (location=%s) back to %s",
              m_location.c_str(), m_event->addr.format().c_str());
    if (newly_created)
      m_context->rsc_manager->erase_server(m_rsc);
    m_context->op->unblock(m_location);
    m_context->op->unblock(Dependency::SERVERS);
    m_context->op->unblock(Dependency::RECOVERY_BLOCKER);
    HT_INFOF("%lld Leaving RegisterServer %s",
            (Lld)header.id, m_rsc->location().c_str());
    return;
  }
  else {
    m_context->monitoring->add_server(m_location, m_system_stats);
    String message = format("Registering server %s\nhost = %s\n"
                            "public addr = %s\nlocal addr = %s\n",
                            m_rsc->location().c_str(),
                            m_system_stats.net_info.host_name.c_str(),
                            m_public_addr.format().c_str(),
                            m_local_addr.format().c_str());

    if (newly_created) {
      String subject = format("Registering server %s - %s (%s)",
                              m_location.c_str(),
                              m_system_stats.net_info.host_name.c_str(),
                              m_public_addr.format().c_str());
      m_context->notification_hook(subject, message);
    }
    boost::replace_all(message, "\n", " ");
    HT_INFOF("%lld %s", (Lld)header.id, message.c_str());

    /** Send back Response **/
    {
      CommHeader header;
      header.initialize_from_request_header(m_event->header);
      header.command = RangeServerProtocol::COMMAND_INITIALIZE;
      m_context->system_state->get(specs, &generation);
      CommBufPtr cbp(new CommBuf(header, encoded_response_length(generation, specs)));
      encode_response(generation, specs, cbp->get_data_ptr_address());
      int error = m_context->comm->send_response(m_event->addr, cbp);
      if (error != Error::OK)
        HT_ERRORF("Problem sending response (location=%s) back to %s",
            m_location.c_str(), m_event->addr.format().c_str());
    }
  }

  if (!m_rsc->get_balanced())
    m_context->balancer->signal_new_server();

  // TODO:  if (m_rsc->needs_persisting()) ...
  m_context->mml_writer->record_state(m_rsc.get());

  m_context->add_available_server(m_location);

  complete_ok();
  m_context->op->unblock(m_location);
  m_context->op->unblock(Dependency::SERVERS);
  m_context->op->unblock(Dependency::RECOVERY_BLOCKER);
  HT_INFOF("%lld Leaving RegisterServer %s", 
          (Lld)header.id, m_rsc->location().c_str());
}

void OperationRegisterServer::display_state(std::ostream &os) {
  os << " location=" << m_location << " host=" << m_system_stats.net_info.host_name;
  os << " register_ts=" << m_register_ts;
  os << " local_addr=" << m_rsc->local_addr().format();
  os << " public_addr=" << m_rsc->public_addr().format() << " ";
}

void OperationRegisterServer::decode_request(const uint8_t **bufp, size_t *remainp) {
  m_location = Serialization::decode_vstr(bufp, remainp);
  m_listen_port = Serialization::decode_i16(bufp, remainp);
  m_system_stats.decode(bufp, remainp);
  m_register_ts = Serialization::decode_i64(bufp, remainp);
}

const String OperationRegisterServer::name() {
  return "OperationRegisterServer";
}

const String OperationRegisterServer::label() {
  return String("RegisterServer ") + m_location;
}

size_t OperationRegisterServer::encoded_response_length(uint64_t generation,
                             std::vector<SystemVariable::Spec> &specs) const {
  size_t length = 4;
  if (m_error == Error::OK) {
    length += Serialization::encoded_length_vstr(m_location);
    length += 8 + SystemVariable::encoded_length_specs(specs);
  }
  else
    length += Serialization::encoded_length_str16(m_error_msg);
  return length;
}

void OperationRegisterServer::encode_response(uint64_t generation,
                             std::vector<SystemVariable::Spec> &specs,
                                              uint8_t **bufp) const {
  Serialization::encode_i32(bufp, m_error);
  if (m_error == Error::OK) {
    Serialization::encode_vstr(bufp, m_location);
    Serialization::encode_i64(bufp, generation);
    SystemVariable::encode_specs(specs, bufp);
  }
  else
    Serialization::encode_str16(bufp, m_error_msg);
}
