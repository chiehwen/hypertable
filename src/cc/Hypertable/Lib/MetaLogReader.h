/* -*- c++ -*-
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

/** @file
 * Declarations for MetaLog::Reader.
 * This file contains declarations for MetaLog::Reader, a class for reading
 * a %MetaLog.
 */

#ifndef HYPERTABLE_METALOGREADER_H
#define HYPERTABLE_METALOGREADER_H

#include "Common/Filesystem.h"
#include "Common/ReferenceCount.h"

#include <map>
#include <vector>

#include "MetaLogDefinition.h"

namespace Hypertable {

  namespace MetaLog {

  /** @addtogroup libHypertable
   * @{
   */

  /** Reads a %MetaLog.  This class is part of a group of generic meta log
   * manipulation classes.  A meta log is a server state log and is currently
   * used by both the RangeServer and the Master servers.  The set of valid
   * MetaLog::Entity classes are defined by a MetaLog::Definition object that
   * is unique to each server.  This class reads a meta log and provides access
   * to the latest versions of live MetaLog entities that have been persisted in
   * the log.
   */
    class Reader : public ReferenceCount {

    public:
      Reader(FilesystemPtr &fs, DefinitionPtr &definition);
      Reader(FilesystemPtr &fs, DefinitionPtr &definition, const String &path);
      void get_entities(std::vector<EntityPtr> &entities);
      void get_all_entities(std::vector<EntityPtr> &entities);
      void reload();
      int32_t next_file_number() { return m_next_filenum; }
      void load_file(const String &fname);
      uint16_t version() { return m_version; }

    private:

      void verify_backup(int32_t file_num);
      void read_header(int fd);

      FilesystemPtr m_fs;
      MetaLog::DefinitionPtr m_definition;
      String m_path;
      int32_t m_next_filenum;
      std::vector<int32_t> m_file_nums;
      std::map<EntityHeader, EntityPtr> m_entity_map;
      std::vector<EntityPtr> m_entities;
      size_t m_cur_offset;
      String m_backup_path;
      uint16_t m_version;
    };

    /// Smart pointer to Reader
    typedef intrusive_ptr<Reader> ReaderPtr;

    /** @}*/
  }
}

#endif // HYPERTABLE_METALOGREADER_H
