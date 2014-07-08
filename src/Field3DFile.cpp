//----------------------------------------------------------------------------//

/*
 * Copyright (c) 2014 Sony Pictures Imageworks Inc., 
 *                    Pixar Animation Studios Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.  Neither the name of Sony Pictures Imageworks nor the
 * names of its contributors may be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//----------------------------------------------------------------------------//

/*! \file Field3DFile.cpp
  \brief Contains implementations of Field3DFile-related member functions
  \ingroup field
*/

//----------------------------------------------------------------------------//

#include "Field3DFile.h"

#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include <boost/tokenizer.hpp>
#include <boost/utility.hpp>

#include "Field.h"
#include "ClassFactory.h"
#include "OArchive.h"
#include "OgIAttribute.h"
#include "OgIDataset.h"
#include "OgIGroup.h"
#include "OgOAttribute.h"
#include "OgODataset.h"
#include "OgOGroup.h"

//----------------------------------------------------------------------------//

using namespace std;

//----------------------------------------------------------------------------//

FIELD3D_NAMESPACE_OPEN

//----------------------------------------------------------------------------//
// Field3D namespaces
//----------------------------------------------------------------------------//

using namespace Exc;

//----------------------------------------------------------------------------//
// Local namespace
//----------------------------------------------------------------------------//

namespace {
  
  // Strings used only in this file --------------------------------------------

  const std::string k_mappingStr("mapping");
  const std::string k_partitionName("partition");  
  const std::string k_versionAttrName("version_number");
  const std::string k_classNameAttrName("class_name");
  const std::string k_mappingTypeAttrName("mapping_type");

  //! This version is stored in every file to determine which library version
  //! produced it.

  V3i k_currentFileVersion = V3i(FIELD3D_MAJOR_VER, 
                                 FIELD3D_MINOR_VER, 
                                 FIELD3D_MICRO_VER);
  int k_minFileVersion[2] = { 0, 0 };

  // Function objects used only in this file -----------------------------------

  std::vector<std::string> makeUnique(std::vector<std::string> vec)
  {
    std::vector<string> ret;
    std::sort(vec.begin(), vec.end());
    std::vector<std::string>::iterator newEnd = 
      std::unique(vec.begin(), vec.end());
    ret.resize(std::distance(vec.begin(), newEnd));
    std::copy(vec.begin(), newEnd, ret.begin()); 
    return ret;
  }

//----------------------------------------------------------------------------//

  //! Functor used with for_each to print a container
  template <class T>
  class print : std::unary_function<T, void>
  {
  public:
    print(int indentAmt)
      : indent(indentAmt)
    { }
    void operator()(const T& x) const
    {
      for (int i = 0; i < indent; i++)
        std::cout << " ";
      std::cout << x << std::endl;
    }
    int indent;
  };

//----------------------------------------------------------------------------//

  /*! \brief checks to see if a file/directory exists or not
    \param[in] filename the file/directory to check
    \retval true if it exists
    \retval false if it does not exist
   */
  bool fileExists(const std::string &filename)
  {
#ifdef WIN32
    struct __stat64 statbuf;
    return (_stat64(filename.c_str(), &statbuf) != -1);
#else
    struct stat statbuf;
    return (stat(filename.c_str(), &statbuf) != -1);
#endif
  }

  /*! \brief wrapper around fileExists. Throws instead if the file
    does not exist.
    \throw NoSuchFileException if the file or directory does not exist
    \param[in] filename the file/directory to check
   */
  void checkFile(const std::string &filename)
  {
    if (!fileExists(filename))
    {
      throw NoSuchFileException(filename);
    }
  }

//----------------------------------------------------------------------------//

  bool isSupportedFileVersion(const int fileVersion[3],
                              const int minVersion[2])
  {
    stringstream currentVersionStr;
    currentVersionStr << k_currentFileVersion[0] << "."
                      << k_currentFileVersion[1] << "."
                      << k_currentFileVersion[2];
    stringstream fileVersionStr;
    fileVersionStr << fileVersion[0] << "."
                   << fileVersion[1] << "."
                   << fileVersion[2];
    stringstream minVersionStr;
    minVersionStr << minVersion[0] << "."
                  << minVersion[1];

    if (fileVersion[0] > k_currentFileVersion[0] ||
        (fileVersion[0] == k_currentFileVersion[0] && 
         fileVersion[1] > k_currentFileVersion[1])) {
      Msg::print(Msg::SevWarning, "File version " + fileVersionStr.str() +
                 " is higher than the current version " +
                 currentVersionStr.str());
      return true;
    }

    if (fileVersion[0] < minVersion[0] ||
        (fileVersion[0] == minVersion[0] &&
         fileVersion[1] < minVersion[1])) {
      Msg::print(Msg::SevWarning, "File version " + fileVersionStr.str() +
                 " is lower than the minimum supported version " +
                 minVersionStr.str());
      return false;
    }
    return true;
  }

//----------------------------------------------------------------------------//

} // end of local namespace

//----------------------------------------------------------------------------//
// File namespace
//----------------------------------------------------------------------------//

namespace File {

//----------------------------------------------------------------------------//
// Partition implementations
//----------------------------------------------------------------------------//

std::string Partition::className() const
{
  return k_partitionName;
}

//----------------------------------------------------------------------------//

void 
Partition::addScalarLayer(const Layer &layer)
{
  m_scalarLayers.push_back(layer);
}

//----------------------------------------------------------------------------//

void 
Partition::addVectorLayer(const Layer &layer)
{
  m_vectorLayers.push_back(layer);
}

//----------------------------------------------------------------------------//

const Layer* 
Partition::scalarLayer(const std::string &name) const
{
  for (ScalarLayerList::const_iterator i = m_scalarLayers.begin();
       i != m_scalarLayers.end(); ++i) {
    if (i->name == name)
      return &(*i);
  }
  return NULL;
}

//----------------------------------------------------------------------------//

const Layer* 
Partition::vectorLayer(const std::string &name) const
{
  for (VectorLayerList::const_iterator i = m_vectorLayers.begin();
       i != m_vectorLayers.end(); ++i) {
    if (i->name == name)
      return &(*i);
  }
  return NULL;
}

//----------------------------------------------------------------------------//

void 
Partition::getScalarLayerNames(std::vector<std::string> &names) const 
{
  // We don't want to do names.clear() here, since this gets called
  // inside some loops that want to accumulate names.
  for (ScalarLayerList::const_iterator i = m_scalarLayers.begin();
       i != m_scalarLayers.end(); ++i) {
    names.push_back(i->name);
  }
}

//----------------------------------------------------------------------------//

void 
Partition::getVectorLayerNames(std::vector<std::string> &names) const
{
  // We don't want to do names.clear() here, since this gets called
  // inside some loops that want to accumulate names.
  for (VectorLayerList::const_iterator i = m_vectorLayers.begin();
       i != m_vectorLayers.end(); ++i) {
    names.push_back(i->name);
  }
}

//----------------------------------------------------------------------------//

} // namespace File

//----------------------------------------------------------------------------//
// Field3DFileBase implementations
//----------------------------------------------------------------------------//

Field3DFileBase::Field3DFileBase()
  : m_metadata(this)
{
  // Empty
}

//----------------------------------------------------------------------------//

Field3DFileBase::~Field3DFileBase()
{
  close();
}

//----------------------------------------------------------------------------//

std::string 
Field3DFileBase::intPartitionName(const std::string &partitionName,
                                  const std::string & /* layerName */,
                                  FieldRes::Ptr field)
{
  // Loop over existing partitions and see if there's a matching mapping
  for (PartitionList::const_iterator i = m_partitions.begin();
       i != m_partitions.end(); ++i) {
    if (removeUniqueId((**i).name) == partitionName) {
      if ((**i).mapping->isIdentical(field->mapping())) {
        return (**i).name;
      }
    }
  }

  // If there was no previously matching name, then make a new one

  int nextIdx = -1;
  if (m_partitionCount.find(partitionName) != m_partitionCount.end()) {
    nextIdx = ++m_partitionCount[partitionName];
  } else {
    nextIdx = 0;
    m_partitionCount[partitionName] = 0;
  }

  return makeIntPartitionName(partitionName, nextIdx);
}

//----------------------------------------------------------------------------//

File::Partition::Ptr Field3DFileBase::partition(const string &partitionName) 
{
  for (PartitionList::iterator i = m_partitions.begin();
       i != m_partitions.end(); ++i) {
    if ((**i).name == partitionName)
      return *i;
  }

  return File::Partition::Ptr();
}

//----------------------------------------------------------------------------//

File::Partition::Ptr
Field3DFileBase::partition(const string &partitionName) const
{
  for (PartitionList::const_iterator i = m_partitions.begin();
       i != m_partitions.end(); ++i) {
    if ((**i).name == partitionName)
      return *i;
  }

  return File::Partition::Ptr();
}

//----------------------------------------------------------------------------//

std::string 
Field3DFileBase::removeUniqueId(const std::string &partitionName) const
{
  size_t pos = partitionName.rfind(".");
  if (pos == partitionName.npos) {
    return partitionName;
  } else {
    return partitionName.substr(0, pos);
  }  
}

//----------------------------------------------------------------------------//

void 
Field3DFileBase::getPartitionNames(vector<string> &names) const
{
  names.clear();

  vector<string> tempNames;

  for (PartitionList::const_iterator i = m_partitions.begin();
       i != m_partitions.end(); ++i) {
    tempNames.push_back(removeUniqueId((**i).name));
  }

  names = makeUnique(tempNames);
}

//----------------------------------------------------------------------------//

void 
Field3DFileBase::getScalarLayerNames(vector<string> &names, 
                                     const string &partitionName) const
{
  names.clear();

  for (int i = 0; i < numIntPartitions(partitionName); i++) {
    string internalName = makeIntPartitionName(partitionName, i);
    File::Partition::Ptr part = partition(internalName);
    if (part)
      part->getScalarLayerNames(names);
  }

  names = makeUnique(names);
}

//----------------------------------------------------------------------------//

void 
Field3DFileBase::getVectorLayerNames(vector<string> &names, 
                                     const string &partitionName) const
{
  names.clear();

  for (int i = 0; i < numIntPartitions(partitionName); i++) {
    string internalName = makeIntPartitionName(partitionName, i);
    File::Partition::Ptr part = partition(internalName);
    if (part)
      part->getVectorLayerNames(names);
  }

  names = makeUnique(names);
}

//----------------------------------------------------------------------------//

void 
Field3DFileBase::getIntPartitionNames(vector<string> &names) const
{
  names.clear();

  for (PartitionList::const_iterator i = m_partitions.begin();
       i != m_partitions.end(); ++i) {
    names.push_back((**i).name);
  }
}

//----------------------------------------------------------------------------//

void 
Field3DFileBase::getIntScalarLayerNames(vector<string> &names, 
                                        const string &intPartitionName) const
{
  names.clear();

  File::Partition::Ptr part = partition(intPartitionName);

  if (!part) {
    Msg::print("getIntScalarLayerNames no partition: " + intPartitionName);
    return;
  }

  part->getScalarLayerNames(names);
}

//----------------------------------------------------------------------------//

void 
Field3DFileBase::getIntVectorLayerNames(vector<string> &names, 
                                        const string &intPartitionName) const
{
  names.clear();

  File::Partition::Ptr part = partition(intPartitionName);

  if (!part) {
    Msg::print("getIntVectorLayerNames no partition: " + intPartitionName);    
    return;
  }

  part->getVectorLayerNames(names);
}

//----------------------------------------------------------------------------//

void Field3DFileBase::clear()
{
  closeInternal();
  m_partitions.clear();
  m_groupMembership.clear();
}

//----------------------------------------------------------------------------//

bool Field3DFileBase::close()
{
  closeInternal();

  return true;
}

//----------------------------------------------------------------------------//

int 
Field3DFileBase::numIntPartitions(const std::string &partitionName) const
{
  int count = 0;

  for (PartitionList::const_iterator i = m_partitions.begin();
       i != m_partitions.end(); ++i) {
    string name = (**i).name;
    size_t pos = name.rfind(".");
    if (pos != name.npos) {
      if (name.substr(0, pos) == partitionName) {
        count++;
      }
    }
  }

  return count;
}

//----------------------------------------------------------------------------//

string 
Field3DFileBase::makeIntPartitionName(const std::string &partitionName,
                                      int i) const
{
  return partitionName + "." + boost::lexical_cast<std::string>(i);
}

//----------------------------------------------------------------------------//

void 
Field3DFileBase::addGroupMembership(const GroupMembershipMap& groupMembers)
{
  GroupMembershipMap::const_iterator i= groupMembers.begin();
  GroupMembershipMap::const_iterator end= groupMembers.end();

  for (; i != end; ++i) {
    GroupMembershipMap::iterator foundGroupIter = 
      m_groupMembership.find(i->first);
    if (foundGroupIter != m_groupMembership.end()){
      std::string value = m_groupMembership[i->first] + i->second;
      m_groupMembership[i->first] = value;
    } else { 
      m_groupMembership[i->first] = i->second;
    }
  }
}

//----------------------------------------------------------------------------//
// Field3DInputFile implementations
//----------------------------------------------------------------------------//

Field3DInputFile::Field3DInputFile() 
{ 
  // Empty
}

//----------------------------------------------------------------------------//

Field3DInputFile::~Field3DInputFile() 
{ 
  clear(); 
}

//----------------------------------------------------------------------------//

bool Field3DInputFile::open(const string &filename)
{
  clear();

  bool success = true;

  // Record filename
  m_filename = filename;

  // Open the Ogawa archive
  m_archive.reset(new Alembic::Ogawa::IArchive(filename));

  return success;
}

//----------------------------------------------------------------------------//
// Field3DOutputFile implementations
//----------------------------------------------------------------------------//

Field3DOutputFile::Field3DOutputFile() 
{ 
  // Empty
}

//----------------------------------------------------------------------------//

Field3DOutputFile::~Field3DOutputFile() 
{ 

}

//----------------------------------------------------------------------------//

bool Field3DOutputFile::create(const string &filename, CreateMode cm)
{
  closeInternal();

  if (cm == FailOnExisting && fileExists(filename)) {
    return false;
  }

  // Create the Ogawa archive
  m_archive.reset(new Alembic::Ogawa::OArchive(filename));

  // Check that it's valid
  if (!m_archive->isValid()) {
    return false;
  }

  // Get the root
  m_root.reset(new OgOGroup(*m_archive));

  // Create the version attribute
#if 0
  OgOAttribute<veci32_t> f3dVersion(*m_root, k_versionAttrName, 
                                    k_currentFileVersion);
#endif

  return true;
}

//----------------------------------------------------------------------------//

bool 
Field3DOutputFile::writeGlobalMetadata()
{

#if 0

  // Add metadata group and write it out  
  H5ScopedGcreate metadataGroup(m_file, "field3d_global_metadata");
  if (metadataGroup.id() < 0) {
    Msg::print(Msg::SevWarning, "Error creating group: file metadata");
    return false;
  }  
  if (!writeMetadata(metadataGroup.id())) {
    Msg::print(Msg::SevWarning, "Error writing file metadata.");
    return false;
  }   
 
#endif
 
  return true;
}

//----------------------------------------------------------------------------//

bool 
Field3DOutputFile::writeGroupMembership()
{

#if 0

  using namespace std;
  using namespace Hdf5Util;

  if (!m_groupMembership.size())
    return true;

  H5ScopedGcreate group(m_file, "field3d_group_membership");
  if (group < 0) {
    Msg::print(Msg::SevWarning, 
               "Error creating field3d_group_membership group.");      
    return false;
  } 

  if (!writeAttribute(group, "is_field3d_group_membership", "1")) {
    Msg::print(Msg::SevWarning, 
               "Failed to write field3d_group_membership attribute.");
    return false;
  }    

  std::map<std::string, std::string>::const_iterator iter = 
    m_groupMembership.begin();
  std::map<std::string, std::string>::const_iterator iEnd = 
    m_groupMembership.end();
  
  for (; iter != iEnd; ++iter) {
    if (!writeAttribute(group, iter->first, iter->second)) {
      Msg::print(Msg::SevWarning, 
                 "Failed to write groupMembership string: "+ iter->first);
      return false;
    }        
  }

#endif
  
  return true;
}

//----------------------------------------------------------------------------//

std::string
Field3DOutputFile::incrementPartitionName(std::string &partitionName)
{
  std::string myPartitionName = removeUniqueId(partitionName);
  int nextIdx = -1;
  if (m_partitionCount.find(myPartitionName) != m_partitionCount.end()) {
    nextIdx = ++m_partitionCount[myPartitionName];
  } else {
    nextIdx = 0;
    m_partitionCount[myPartitionName] = 0;
  }

  return makeIntPartitionName(myPartitionName, nextIdx);
}

//----------------------------------------------------------------------------//

FIELD3D_NAMESPACE_SOURCE_CLOSE

//----------------------------------------------------------------------------//
