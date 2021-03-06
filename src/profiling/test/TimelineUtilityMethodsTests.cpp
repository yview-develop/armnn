//
// Copyright © 2019 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "SendCounterPacketTests.hpp"

#include <SendTimelinePacket.hpp>
#include <TimelineUtilityMethods.hpp>
#include <LabelsAndEventClasses.hpp>
#include <ProfilingService.hpp>

#include <boost/test/unit_test.hpp>

using namespace armnn;
using namespace armnn::profiling;

namespace
{

inline unsigned int OffsetToNextWord(unsigned int numberOfBytes)
{
    unsigned int uint32_t_size = sizeof(uint32_t);

    unsigned int remainder = numberOfBytes % uint32_t_size;
    if (remainder == 0)
    {
        return numberOfBytes;
    }

    return numberOfBytes + uint32_t_size - remainder;
}

void VerifyTimelineLabelBinaryPacket(Optional<ProfilingGuid> guid,
                                     const std::string& label,
                                     const unsigned char* readableData,
                                     unsigned int& offset)
{
    BOOST_ASSERT(readableData);

    // Utils
    unsigned int uint32_t_size = sizeof(uint32_t);
    unsigned int uint64_t_size = sizeof(uint64_t);
    unsigned int label_size    = boost::numeric_cast<unsigned int>(label.size());

    // Check the TimelineLabelBinaryPacket header
    uint32_t entityBinaryPacketHeaderWord0 = ReadUint32(readableData, offset);
    uint32_t entityBinaryPacketFamily      = (entityBinaryPacketHeaderWord0 >> 26) & 0x0000003F;
    uint32_t entityBinaryPacketClass       = (entityBinaryPacketHeaderWord0 >> 19) & 0x0000007F;
    uint32_t entityBinaryPacketType        = (entityBinaryPacketHeaderWord0 >> 16) & 0x00000007;
    uint32_t entityBinaryPacketStreamId    = (entityBinaryPacketHeaderWord0 >>  0) & 0x00000007;
    BOOST_CHECK(entityBinaryPacketFamily   == 1);
    BOOST_CHECK(entityBinaryPacketClass    == 0);
    BOOST_CHECK(entityBinaryPacketType     == 1);
    BOOST_CHECK(entityBinaryPacketStreamId == 0);
    offset += uint32_t_size;
    uint32_t entityBinaryPacketHeaderWord1   = ReadUint32(readableData, offset);
    uint32_t eventBinaryPacketSequenceNumber = (entityBinaryPacketHeaderWord1 >> 24) & 0x00000001;
    uint32_t eventBinaryPacketDataLength     = (entityBinaryPacketHeaderWord1 >>  0) & 0x00FFFFFF;
    BOOST_CHECK(eventBinaryPacketSequenceNumber == 0);
    BOOST_CHECK(eventBinaryPacketDataLength     == 16 + OffsetToNextWord(label_size + 1));

    // Check the decl id
    offset += uint32_t_size;
    uint32_t eventClassDeclId = ReadUint32(readableData, offset);
    BOOST_CHECK(eventClassDeclId == 0);

    // Check the profiling GUID
    offset += uint32_t_size;
    uint64_t readProfilingGuid = ReadUint64(readableData, offset);
    if (guid.has_value())
    {
        BOOST_CHECK(readProfilingGuid == guid.value());
    }
    else
    {
        BOOST_CHECK(readProfilingGuid == ProfilingService::Instance().GenerateStaticId(label));
    }

    // Check the SWTrace label
    offset += uint64_t_size;
    uint32_t swTraceLabelLength = ReadUint32(readableData, offset);
    BOOST_CHECK(swTraceLabelLength == label_size + 1); // Label length including the null-terminator
    offset += uint32_t_size;
    BOOST_CHECK(std::memcmp(readableData + offset,                  // Offset to the label in the buffer
                            label.data(),                           // The original label
                            swTraceLabelLength - 1) == 0);          // The length of the label
    BOOST_CHECK(readableData[offset + swTraceLabelLength] == '\0'); // The null-terminator

    // SWTrace strings are written in blocks of words, so the offset has to be updated to the next whole word
    offset += OffsetToNextWord(swTraceLabelLength);
}

void VerifyTimelineEventClassBinaryPacket(ProfilingGuid guid,
                                          const unsigned char* readableData,
                                          unsigned int& offset)
{
    BOOST_ASSERT(readableData);

    // Utils
    unsigned int uint32_t_size = sizeof(uint32_t);
    unsigned int uint64_t_size = sizeof(uint64_t);

    // Check the TimelineEventClassBinaryPacket header
    uint32_t entityBinaryPacketHeaderWord0 = ReadUint32(readableData, offset);
    uint32_t entityBinaryPacketFamily      = (entityBinaryPacketHeaderWord0 >> 26) & 0x0000003F;
    uint32_t entityBinaryPacketClass       = (entityBinaryPacketHeaderWord0 >> 19) & 0x0000007F;
    uint32_t entityBinaryPacketType        = (entityBinaryPacketHeaderWord0 >> 16) & 0x00000007;
    uint32_t entityBinaryPacketStreamId    = (entityBinaryPacketHeaderWord0 >>  0) & 0x00000007;
    BOOST_CHECK(entityBinaryPacketFamily   == 1);
    BOOST_CHECK(entityBinaryPacketClass    == 0);
    BOOST_CHECK(entityBinaryPacketType     == 1);
    BOOST_CHECK(entityBinaryPacketStreamId == 0);
    offset += uint32_t_size;
    uint32_t entityBinaryPacketHeaderWord1   = ReadUint32(readableData, offset);
    uint32_t eventBinaryPacketSequenceNumber = (entityBinaryPacketHeaderWord1 >> 24) & 0x00000001;
    uint32_t eventBinaryPacketDataLength     = (entityBinaryPacketHeaderWord1 >>  0) & 0x00FFFFFF;
    BOOST_CHECK(eventBinaryPacketSequenceNumber == 0);
    BOOST_CHECK(eventBinaryPacketDataLength     == 12);

    // Check the decl id
    offset += uint32_t_size;
    uint32_t eventClassDeclId = ReadUint32(readableData, offset);
    BOOST_CHECK(eventClassDeclId == 2);

    // Check the profiling GUID
    offset += uint32_t_size;
    uint64_t readProfilingGuid = ReadUint64(readableData, offset);
    BOOST_CHECK(readProfilingGuid == guid);

    // Update the offset to allow parsing to be continued after this function returns
    offset += uint64_t_size;
}

void VerifyTimelineRelationshipBinaryPacket(ProfilingRelationshipType relationshipType,
                                            Optional<ProfilingGuid> relationshipGuid,
                                            Optional<ProfilingGuid> headGuid,
                                            Optional<ProfilingGuid> tailGuid,
                                            const unsigned char* readableData,
                                            unsigned int& offset)
{
    BOOST_ASSERT(readableData);

    uint32_t relationshipTypeUint = 0;
    switch (relationshipType)
    {
        case ProfilingRelationshipType::RetentionLink:
            relationshipTypeUint = 0;
            break;
        case ProfilingRelationshipType::ExecutionLink:
            relationshipTypeUint = 1;
            break;
        case ProfilingRelationshipType::DataLink:
            relationshipTypeUint = 2;
            break;
        case ProfilingRelationshipType::LabelLink:
            relationshipTypeUint = 3;
            break;
        default:
            BOOST_ERROR("Unknown relationship type");
    }

    // Utils
    unsigned int uint32_t_size = sizeof(uint32_t);
    unsigned int uint64_t_size = sizeof(uint64_t);

    // Check the TimelineLabelBinaryPacket header
    uint32_t entityBinaryPacketHeaderWord0 = ReadUint32(readableData, offset);
    uint32_t entityBinaryPacketFamily      = (entityBinaryPacketHeaderWord0 >> 26) & 0x0000003F;
    uint32_t entityBinaryPacketClass       = (entityBinaryPacketHeaderWord0 >> 19) & 0x0000007F;
    uint32_t entityBinaryPacketType        = (entityBinaryPacketHeaderWord0 >> 16) & 0x00000007;
    uint32_t entityBinaryPacketStreamId    = (entityBinaryPacketHeaderWord0 >>  0) & 0x00000007;
    BOOST_CHECK(entityBinaryPacketFamily   == 1);
    BOOST_CHECK(entityBinaryPacketClass    == 0);
    BOOST_CHECK(entityBinaryPacketType     == 1);
    BOOST_CHECK(entityBinaryPacketStreamId == 0);
    offset += uint32_t_size;
    uint32_t entityBinaryPacketHeaderWord1   = ReadUint32(readableData, offset);
    uint32_t eventBinaryPacketSequenceNumber = (entityBinaryPacketHeaderWord1 >> 24) & 0x00000001;
    uint32_t eventBinaryPacketDataLength     = (entityBinaryPacketHeaderWord1 >>  0) & 0x00FFFFFF;
    BOOST_CHECK(eventBinaryPacketSequenceNumber == 0);
    BOOST_CHECK(eventBinaryPacketDataLength     == 32);

    // Check the decl id
    offset += uint32_t_size;
    uint32_t eventClassDeclId = ReadUint32(readableData, offset);
    BOOST_CHECK(eventClassDeclId == 3);

    // Check the relationship type
    offset += uint32_t_size;
    uint32_t readRelationshipTypeUint = ReadUint32(readableData, offset);
    BOOST_CHECK(readRelationshipTypeUint == relationshipTypeUint);

    // Check the relationship GUID
    offset += uint32_t_size;
    uint64_t readRelationshipGuid = ReadUint64(readableData, offset);
    if (relationshipGuid.has_value())
    {
        BOOST_CHECK(readRelationshipGuid == relationshipGuid.value());
    }
    else
    {
        BOOST_CHECK(readRelationshipGuid != ProfilingGuid(0));
    }

    // Check the head of relationship GUID
    offset += uint64_t_size;
    uint64_t readHeadRelationshipGuid = ReadUint64(readableData, offset);
    if (headGuid.has_value())
    {
        BOOST_CHECK(readHeadRelationshipGuid == headGuid.value());
    }
    else
    {
        BOOST_CHECK(readHeadRelationshipGuid != ProfilingGuid(0));
    }

    // Check the tail of relationship GUID
    offset += uint64_t_size;
    uint64_t readTailRelationshipGuid = ReadUint64(readableData, offset);
    if (tailGuid.has_value())
    {
        BOOST_CHECK(readTailRelationshipGuid == tailGuid.value());
    }
    else
    {
        BOOST_CHECK(readTailRelationshipGuid != ProfilingGuid(0));
    }

    // Update the offset to allow parsing to be continued after this function returns
    offset += uint64_t_size;
}

void VerifyTimelineEntityBinaryPacket(Optional<ProfilingGuid> guid,
                                      const unsigned char* readableData,
                                      unsigned int& offset)
{
    BOOST_ASSERT(readableData);

    // Utils
    unsigned int uint32_t_size = sizeof(uint32_t);
    unsigned int uint64_t_size = sizeof(uint64_t);

    // Reading TimelineEntityClassBinaryPacket
    uint32_t entityBinaryPacketHeaderWord0 = ReadUint32(readableData, offset);
    uint32_t entityBinaryPacketFamily = (entityBinaryPacketHeaderWord0 >> 26) & 0x0000003F;
    uint32_t entityBinaryPacketClass = (entityBinaryPacketHeaderWord0 >> 19) & 0x0000007F;
    uint32_t entityBinaryPacketType = (entityBinaryPacketHeaderWord0 >> 16) & 0x00000007;
    uint32_t entityBinaryPacketStreamId = (entityBinaryPacketHeaderWord0 >>  0) & 0x00000007;

    BOOST_CHECK(entityBinaryPacketFamily == 1);
    BOOST_CHECK(entityBinaryPacketClass  == 0);
    BOOST_CHECK(entityBinaryPacketType   == 1);
    BOOST_CHECK(entityBinaryPacketStreamId     == 0);

    offset += uint32_t_size;
    uint32_t entityBinaryPacketHeaderWord1 = ReadUint32(readableData, offset);
    uint32_t entityBinaryPacketSequenceNumbered = (entityBinaryPacketHeaderWord1 >> 24) & 0x00000001;
    uint32_t entityBinaryPacketDataLength       = (entityBinaryPacketHeaderWord1 >>  0) & 0x00FFFFFF;
    BOOST_CHECK(entityBinaryPacketSequenceNumbered == 0);
    BOOST_CHECK(entityBinaryPacketDataLength       == 12);

    // Check the decl_id
    offset += uint32_t_size;
    uint32_t entityDeclId = ReadUint32(readableData, offset);
    BOOST_CHECK(entityDeclId == 1);

    // Check the profiling GUID
    offset += uint32_t_size;
    uint64_t readProfilingGuid = ReadUint64(readableData, offset);

    if (guid.has_value())
    {
        BOOST_CHECK(readProfilingGuid == guid.value());
    }
    else
    {
        BOOST_CHECK(readProfilingGuid != ProfilingGuid(0));
    }

    offset += uint64_t_size;
}

void VerifyTimelineEventBinaryPacket(Optional<uint64_t> timestamp,
                                     Optional<std::thread::id> threadId,
                                     Optional<ProfilingGuid> eventGuid,
                                     const unsigned char* readableData,
                                     unsigned int& offset)
{
    BOOST_ASSERT(readableData);

    // Utils
    unsigned int uint32_t_size = sizeof(uint32_t);
    unsigned int uint64_t_size = sizeof(uint64_t);
    unsigned int threadId_size = sizeof(std::thread::id);

    // Reading TimelineEventBinaryPacket
    uint32_t entityBinaryPacketHeaderWord0 = ReadUint32(readableData, offset);
    uint32_t entityBinaryPacketFamily   = (entityBinaryPacketHeaderWord0 >> 26) & 0x0000003F;
    uint32_t entityBinaryPacketClass    = (entityBinaryPacketHeaderWord0 >> 19) & 0x0000007F;
    uint32_t entityBinaryPacketType     = (entityBinaryPacketHeaderWord0 >> 16) & 0x00000007;
    uint32_t entityBinaryPacketStreamId = (entityBinaryPacketHeaderWord0 >>  0) & 0x00000007;

    BOOST_CHECK(entityBinaryPacketFamily   == 1);
    BOOST_CHECK(entityBinaryPacketClass    == 0);
    BOOST_CHECK(entityBinaryPacketType     == 1);
    BOOST_CHECK(entityBinaryPacketStreamId == 0);

    offset += uint32_t_size;
    uint32_t entityBinaryPacketHeaderWord1 = ReadUint32(readableData, offset);
    uint32_t entityBinaryPacketSequenceNumbered = (entityBinaryPacketHeaderWord1 >> 24) & 0x00000001;
    uint32_t entityBinaryPacketDataLength       = (entityBinaryPacketHeaderWord1 >>  0) & 0x00FFFFFF;
    BOOST_CHECK(entityBinaryPacketSequenceNumbered == 0);
    BOOST_CHECK(entityBinaryPacketDataLength       == 20 + threadId_size);

    // Check the decl_id
    offset += uint32_t_size;
    uint32_t entityDeclId = ReadUint32(readableData, offset);
    BOOST_CHECK(entityDeclId == 4);

    // Check the timestamp
    offset += uint32_t_size;
    uint64_t readTimestamp = ReadUint64(readableData, offset);
    if (timestamp.has_value())
    {
        BOOST_CHECK(readTimestamp == timestamp.value());
    }
    else
    {
        BOOST_CHECK(readTimestamp != 0);
    }

    // Check the thread id
    offset += uint64_t_size;
    std::vector<uint8_t> readThreadId(threadId_size, 0);
    ReadBytes(readableData, offset, threadId_size, readThreadId.data());
    if (threadId.has_value())
    {
        BOOST_CHECK(readThreadId == threadId.value());
    }
    else
    {
        BOOST_CHECK(readThreadId == std::this_thread::get_id());
    }

    // Check the event GUID
    offset += threadId_size;
    uint64_t readEventGuid = ReadUint64(readableData, offset);
    if (eventGuid.has_value())
    {
        BOOST_CHECK(readEventGuid == eventGuid.value());
    }
    else
    {
        BOOST_CHECK(readEventGuid != ProfilingGuid(0));
    }

    offset += uint64_t_size;
}

} // Anonymous namespace

BOOST_AUTO_TEST_SUITE(TimelineUtilityMethodsTests)

BOOST_AUTO_TEST_CASE(CreateTypedLabelTest)
{
    MockBufferManager mockBufferManager(1024);
    SendTimelinePacket sendTimelinePacket(mockBufferManager);
    TimelineUtilityMethods timelineUtilityMethods(sendTimelinePacket);

    // Generate first guid to ensure that the named typed entity guid is not 0 on local single test.
    ProfilingService::Instance().NextGuid();

    ProfilingGuid entityGuid(123);
    const std::string entityName = "some entity";
    ProfilingStaticGuid labelTypeGuid(456);

    BOOST_CHECK_NO_THROW(timelineUtilityMethods.CreateTypedLabel(entityGuid, entityName, labelTypeGuid));

    // Commit all packets at once
    sendTimelinePacket.Commit();

    // Get the readable buffer
    auto readableBuffer = mockBufferManager.GetReadableBuffer();
    BOOST_CHECK(readableBuffer != nullptr);
    unsigned int size = readableBuffer->GetSize();
    BOOST_CHECK(size == 116);
    const unsigned char* readableData = readableBuffer->GetReadableData();
    BOOST_CHECK(readableData != nullptr);

    // Utils
    unsigned int offset = 0;

    // First packet sent: TimelineLabelBinaryPacket
    VerifyTimelineLabelBinaryPacket(EmptyOptional(), entityName, readableData, offset);

    // Second packet sent: TimelineRelationshipBinaryPacket
    VerifyTimelineRelationshipBinaryPacket(ProfilingRelationshipType::LabelLink,
                                           EmptyOptional(),
                                           entityGuid,
                                           EmptyOptional(),
                                           readableData,
                                           offset);

    // Third packet sent: TimelineRelationshipBinaryPacket
    VerifyTimelineRelationshipBinaryPacket(ProfilingRelationshipType::LabelLink,
                                           EmptyOptional(),
                                           EmptyOptional(),
                                           labelTypeGuid,
                                           readableData,
                                           offset);

    // Mark the buffer as read
    mockBufferManager.MarkRead(readableBuffer);
}

BOOST_AUTO_TEST_CASE(SendWellKnownLabelsAndEventClassesTest)
{
    MockBufferManager mockBufferManager(1024);
    SendTimelinePacket sendTimelinePacket(mockBufferManager);
    TimelineUtilityMethods timelineUtilityMethods(sendTimelinePacket);

    BOOST_CHECK_NO_THROW(timelineUtilityMethods.SendWellKnownLabelsAndEventClasses());

    // Commit all packets at once
    sendTimelinePacket.Commit();

    // Get the readable buffer
    auto readableBuffer = mockBufferManager.GetReadableBuffer();
    BOOST_CHECK(readableBuffer != nullptr);
    unsigned int size = readableBuffer->GetSize();
    BOOST_CHECK(size == 136);
    const unsigned char* readableData = readableBuffer->GetReadableData();
    BOOST_CHECK(readableData != nullptr);

    // Utils
    unsigned int offset = 0;

    // First "well-known" label: NAME
    VerifyTimelineLabelBinaryPacket(LabelsAndEventClasses::NAME_GUID,
                                    LabelsAndEventClasses::NAME_LABEL,
                                    readableData,
                                    offset);

    // Second "well-known" label: TYPE
    VerifyTimelineLabelBinaryPacket(LabelsAndEventClasses::TYPE_GUID,
                                    LabelsAndEventClasses::TYPE_LABEL,
                                    readableData,
                                    offset);

    // Third "well-known" label: INDEX
    VerifyTimelineLabelBinaryPacket(LabelsAndEventClasses::INDEX_GUID,
                                    LabelsAndEventClasses::INDEX_LABEL,
                                    readableData,
                                    offset);

    // First "well-known" event class: START OF LIFE
    VerifyTimelineEventClassBinaryPacket(LabelsAndEventClasses::ARMNN_PROFILING_SOL_EVENT_CLASS,
                                         readableData,
                                         offset);

    // Second "well-known" event class: END OF LIFE
    VerifyTimelineEventClassBinaryPacket(LabelsAndEventClasses::ARMNN_PROFILING_EOL_EVENT_CLASS,
                                         readableData,
                                         offset);

    // Mark the buffer as read
    mockBufferManager.MarkRead(readableBuffer);
}

BOOST_AUTO_TEST_CASE(CreateNamedTypedChildEntityTest)
{
    MockBufferManager mockBufferManager(1024);
    SendTimelinePacket sendTimelinePacket(mockBufferManager);
    TimelineUtilityMethods timelineUtilityMethods(sendTimelinePacket);

    ProfilingDynamicGuid childEntityGuid(0);
    ProfilingGuid parentEntityGuid(123);
    const std::string entityName = "some entity";
    const std::string entityType = "some type";

    // Generate first guid to ensure that the named typed entity guid is not 0 on local single test.
    ProfilingService::Instance().NextGuid();

    BOOST_CHECK_THROW(timelineUtilityMethods.CreateNamedTypedChildEntity(parentEntityGuid, "", entityType),
                      InvalidArgumentException);
    BOOST_CHECK_THROW(timelineUtilityMethods.CreateNamedTypedChildEntity(parentEntityGuid, entityName, ""),
                      InvalidArgumentException);
    BOOST_CHECK_THROW(timelineUtilityMethods.CreateNamedTypedChildEntity(
        childEntityGuid, parentEntityGuid, "", entityType), InvalidArgumentException);
    BOOST_CHECK_THROW(timelineUtilityMethods.CreateNamedTypedChildEntity(
        childEntityGuid, parentEntityGuid, entityName, ""), InvalidArgumentException);

    BOOST_CHECK_NO_THROW(childEntityGuid = timelineUtilityMethods.CreateNamedTypedChildEntity(parentEntityGuid,
                                                                                              entityName,
                                                                                              entityType));
    BOOST_CHECK(childEntityGuid != ProfilingGuid(0));

    // Commit all packets at once
    sendTimelinePacket.Commit();

    // Get the readable buffer
    auto readableBuffer = mockBufferManager.GetReadableBuffer();
    BOOST_CHECK(readableBuffer != nullptr);
    unsigned int size = readableBuffer->GetSize();
    BOOST_CHECK(size == 292);
    const unsigned char* readableData = readableBuffer->GetReadableData();
    BOOST_CHECK(readableData != nullptr);

    // Utils
    unsigned int offset = 0;

    // First packet sent: TimelineEntityBinaryPacket
    VerifyTimelineEntityBinaryPacket(EmptyOptional(), readableData, offset);

    // Second packet sent: TimelineLabelBinaryPacket
    VerifyTimelineLabelBinaryPacket(EmptyOptional(), entityName, readableData, offset);

    // Third packet sent: TimelineRelationshipBinaryPacket
    VerifyTimelineRelationshipBinaryPacket(ProfilingRelationshipType::LabelLink,
                                           EmptyOptional(),
                                           EmptyOptional(),
                                           EmptyOptional(),
                                           readableData,
                                           offset);

    // Fourth packet sent: TimelineRelationshipBinaryPacket
    VerifyTimelineRelationshipBinaryPacket(ProfilingRelationshipType::LabelLink,
                                           EmptyOptional(),
                                           EmptyOptional(),
                                           LabelsAndEventClasses::NAME_GUID,
                                           readableData,
                                           offset);

    // Fifth packet sent: TimelineLabelBinaryPacket
    VerifyTimelineLabelBinaryPacket(EmptyOptional(), entityType, readableData, offset);

    // Sixth packet sent: TimelineRelationshipBinaryPacket
    VerifyTimelineRelationshipBinaryPacket(ProfilingRelationshipType::LabelLink,
                                           EmptyOptional(),
                                           EmptyOptional(),
                                           EmptyOptional(),
                                           readableData,
                                           offset);

    // Seventh packet sent: TimelineRelationshipBinaryPacket
    VerifyTimelineRelationshipBinaryPacket(ProfilingRelationshipType::LabelLink,
                                           EmptyOptional(),
                                           EmptyOptional(),
                                           LabelsAndEventClasses::TYPE_GUID,
                                           readableData,
                                           offset);

    // Eighth packet sent: TimelineRelationshipBinaryPacket
    VerifyTimelineRelationshipBinaryPacket(ProfilingRelationshipType::RetentionLink,
                                           EmptyOptional(),
                                           parentEntityGuid,
                                           EmptyOptional(),
                                           readableData,
                                           offset);

    // Mark the buffer as read
    mockBufferManager.MarkRead(readableBuffer);
}

BOOST_AUTO_TEST_CASE(DeclareLabelTest)
{
    MockBufferManager mockBufferManager(1024);
    SendTimelinePacket sendTimelinePacket(mockBufferManager);
    TimelineUtilityMethods timelineUtilityMethods(sendTimelinePacket);

    // Generate first guid to ensure that the named typed entity guid is not 0 on local single test.
    ProfilingService::Instance().NextGuid();

    // Try declaring an invalid (empty) label
    BOOST_CHECK_THROW(timelineUtilityMethods.DeclareLabel(""), InvalidArgumentException);

    // Try declaring an invalid (wrong SWTrace format) label
    BOOST_CHECK_THROW(timelineUtilityMethods.DeclareLabel("inv@lid lab€l"), RuntimeException);

    // Declare a valid label
    const std::string labelName = "valid label";
    ProfilingGuid labelGuid = 0;
    BOOST_CHECK_NO_THROW(labelGuid = timelineUtilityMethods.DeclareLabel(labelName));
    BOOST_CHECK(labelGuid != ProfilingGuid(0));

    // Try adding the same label as before
    ProfilingGuid newLabelGuid = 0;
    BOOST_CHECK_NO_THROW(newLabelGuid = timelineUtilityMethods.DeclareLabel(labelName));
    BOOST_CHECK(newLabelGuid != ProfilingGuid(0));
    BOOST_CHECK(newLabelGuid == labelGuid);
}

BOOST_AUTO_TEST_CASE(CreateNameTypeEntityInvalidTest)
{
    MockBufferManager mockBufferManager(1024);
    SendTimelinePacket sendTimelinePacket(mockBufferManager);
    TimelineUtilityMethods timelineUtilityMethods(sendTimelinePacket);

    // Invalid name
    BOOST_CHECK_THROW(timelineUtilityMethods.CreateNamedTypedEntity("", "Type"), InvalidArgumentException);

    // Invalid type
    BOOST_CHECK_THROW(timelineUtilityMethods.CreateNamedTypedEntity("Name", ""), InvalidArgumentException);

    ProfilingDynamicGuid guid = ProfilingService::Instance().NextGuid();

    // CreatedNamedTypedEntity with Guid - Invalid name
    BOOST_CHECK_THROW(timelineUtilityMethods.CreateNamedTypedEntity(guid, "", "Type"),
                      InvalidArgumentException);

    // CreatedNamedTypedEntity with Guid - Invalid type
    BOOST_CHECK_THROW(timelineUtilityMethods.CreateNamedTypedEntity(guid, "Name", ""),
                      InvalidArgumentException);

}

BOOST_AUTO_TEST_CASE(CreateNameTypeEntityTest)
{
    MockBufferManager mockBufferManager(1024);
    SendTimelinePacket sendTimelinePacket(mockBufferManager);
    TimelineUtilityMethods timelineUtilityMethods(sendTimelinePacket);

    const std::string entityName = "Entity0";
    const std::string entityType = "Type0";

    // Generate first guid to ensure that the named typed entity guid is not 0 on local single test.
    ProfilingService::Instance().NextGuid();

    ProfilingDynamicGuid guid = timelineUtilityMethods.CreateNamedTypedEntity(entityName, entityType);
    BOOST_CHECK(guid != ProfilingGuid(0));

    // Commit all packets at once
    sendTimelinePacket.Commit();

    // Get the readable buffer
    auto readableBuffer = mockBufferManager.GetReadableBuffer();
    BOOST_CHECK(readableBuffer != nullptr);
    unsigned int size = readableBuffer->GetSize();
    BOOST_CHECK(size == 244);
    const unsigned char* readableData = readableBuffer->GetReadableData();
    BOOST_CHECK(readableData != nullptr);

    // Utils
    unsigned int offset = 0;

    // First packet sent: TimelineEntityBinaryPacket
    VerifyTimelineEntityBinaryPacket(guid, readableData, offset);

    // Packets for Name Entity
    // First packet sent: TimelineLabelBinaryPacket
    VerifyTimelineLabelBinaryPacket(EmptyOptional(), entityName, readableData, offset);

    // Second packet sent: TimelineRelationshipBinaryPacket
    VerifyTimelineRelationshipBinaryPacket(ProfilingRelationshipType::LabelLink,
                                           EmptyOptional(),
                                           EmptyOptional(),
                                           EmptyOptional(),
                                           readableData,
                                           offset);

    // Third packet sent: TimelineRelationshipBinaryPacket
    VerifyTimelineRelationshipBinaryPacket(ProfilingRelationshipType::LabelLink,
                                           EmptyOptional(),
                                           EmptyOptional(),
                                           LabelsAndEventClasses::NAME_GUID,
                                           readableData,
                                           offset);

    // Packets for Type Entity
    // First packet sent: TimelineLabelBinaryPacket
    VerifyTimelineLabelBinaryPacket(EmptyOptional(), entityType, readableData, offset);

    // Second packet sent: TimelineRelationshipBinaryPacket
    VerifyTimelineRelationshipBinaryPacket(ProfilingRelationshipType::LabelLink,
                                           EmptyOptional(),
                                           EmptyOptional(),
                                           EmptyOptional(),
                                           readableData,
                                           offset);

    // Third packet sent: TimelineRelationshipBinaryPacket
    VerifyTimelineRelationshipBinaryPacket(ProfilingRelationshipType::LabelLink,
                                           EmptyOptional(),
                                           EmptyOptional(),
                                           LabelsAndEventClasses::TYPE_GUID,
                                           readableData,
                                           offset);

    // Mark the buffer as read
    mockBufferManager.MarkRead(readableBuffer);
}

BOOST_AUTO_TEST_CASE(RecordEventTest)
{
    MockBufferManager mockBufferManager(1024);
    SendTimelinePacket sendTimelinePacket(mockBufferManager);
    TimelineUtilityMethods timelineUtilityMethods(sendTimelinePacket);

    // Generate first guid to ensure that the named typed entity guid is not 0 on local single test.
    ProfilingService::Instance().NextGuid();

    ProfilingGuid entityGuid(123);
    ProfilingStaticGuid eventClassGuid(456);
    ProfilingDynamicGuid eventGuid(0);
    BOOST_CHECK_NO_THROW(eventGuid = timelineUtilityMethods.RecordEvent(entityGuid, eventClassGuid));
    BOOST_CHECK(eventGuid != ProfilingGuid(0));

    // Commit all packets at once
    sendTimelinePacket.Commit();

    // Get the readable buffer
    auto readableBuffer = mockBufferManager.GetReadableBuffer();
    BOOST_CHECK(readableBuffer != nullptr);
    unsigned int size = readableBuffer->GetSize();
    BOOST_CHECK(size == 116);
    const unsigned char* readableData = readableBuffer->GetReadableData();
    BOOST_CHECK(readableData != nullptr);

    // Utils
    unsigned int offset = 0;

    // First packet sent: TimelineEntityBinaryPacket
    VerifyTimelineEventBinaryPacket(EmptyOptional(), EmptyOptional(), EmptyOptional(), readableData, offset);

    // Second packet sent: TimelineRelationshipBinaryPacket
    VerifyTimelineRelationshipBinaryPacket(ProfilingRelationshipType::ExecutionLink,
                                           EmptyOptional(),
                                           entityGuid,
                                           EmptyOptional(),
                                           readableData,
                                           offset);

    // Third packet sent: TimelineRelationshipBinaryPacket
    VerifyTimelineRelationshipBinaryPacket(ProfilingRelationshipType::DataLink,
                                           EmptyOptional(),
                                           entityGuid,
                                           eventClassGuid,
                                           readableData,
                                           offset);

    // Mark the buffer as read
    mockBufferManager.MarkRead(readableBuffer);
}

BOOST_AUTO_TEST_SUITE_END()
