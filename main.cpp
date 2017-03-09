#include <iostream>
#include <sstream>
#include <iomanip>
#include <fcntl.h>
#include <zconf.h>
#include <dvbpsi/dvbpsi.h>
#include <dvbpsi/descriptor.h>
#include <dvbpsi/demux.h>
#include <dvbpsi/eit.h>
#include <dvbpsi/nit.h>
#include <dvbpsi/pat.h>

using namespace std;

enum class PID : uint16_t
{
    PAT = 0x0000,
    CAT = 0x0001,
    TSDT = 0x0002,
    IPMPCIT = 0x0003,
    // 0x0004-0x000F    Reserved for future use
    NIT = 0x0010,
    SDT = 0x0011,
    BAT = 0x0011,
    EIT = 0x0012,
    CIT = 0x0012,
    RST = 0x0013,
    TDT = 0x0014,
    TOT = 0x0014,
    SYNC = 0x0015,
    RNT = 0x0016,
    // 0x0017-0x001B:   Reserved for future use
    // 0x001C: inband signalling
    // 0x001D: measurement
    DIT = 0x001E,
    SIT = 0x001F,
};

enum class SubTable : uint8_t
{
    ProgramAssociation = 0x00,
    ConditionalAccess = 0x01,
    ProgramMap = 0x02,
    TSDescription = 0x03,
    NetworkInformationActual = 0x40,
    NetworkInformationOther = 0x41,
    ServiceDescriptionActualTS = 0x42,
    ServiceDescriptionOtherTS = 0x46,
    BouquetAssociation = 0x4A,
    UpdateNotificationTable = 0x4B,
    EventInformationActualTS = 0x4E,
    EventInformationOtherTS = 0x4F,
    EventInformationActualTSNext = 0x50,
    EventInformationOtherTSNext = 0x51,
};

enum class DescriptorTag : uint8_t
{
    ShortEventDescriptor = 0x4D,
    ExtendedEventDescriptor = 0x4E,
    TimeShiftDescriptor = 0x4F,
    ComponentDescriptor = 0x50,
};

enum class StreamContentType : uint8_t
{
    Reserved = 0x0,
    MPEG2Video = 0x1,
    MPEGAudio = 0x2,
    DVBSubtitles = 0x3,
    AC3Audio = 0x4,
    H264Video = 0x5,
    HE_AACAudio = 0x6,
    DTSAudio = 0x7,
    DVB_SRMData = 0x8,
    HEVCVideo = 0x9,
    Reserved2 = 0xA,
    Reserved3 = 0xB,
    UserDefined1 = 0xC,
    UserDefined2 = 0xD,
    UserDefined3 = 0xE,
    UserDefined4 = 0xF,
};

class TransportStreamReader
{
public:
    static const size_t PacketSize = 188;
    static const uint8_t PacketHeaderByte = 0x47;

    TransportStreamReader(int fileHandle);
    ~TransportStreamReader() {}

    bool ReadPacket(uint8_t * buffer);

private:
    int _fileHandle;
};

TransportStreamReader::TransportStreamReader(int fileHandle)
    : _fileHandle(fileHandle)
{
}

bool TransportStreamReader::ReadPacket(uint8_t * buffer)
{
    ssize_t bytesRead = 1;
    size_t bytesToRead = PacketSize - bytesRead;

    buffer[0] = 0;

    while ((buffer[0] != PacketHeaderByte) && (bytesRead > 0))
    {
        bytesRead = read(_fileHandle, buffer, 1);
    }

    while ((bytesToRead != 0) && (bytesRead > 0))
    {
        bytesRead = read(_fileHandle, buffer + 188 - bytesToRead, bytesToRead);
        if (bytesRead >= 0)
            bytesToRead -= bytesRead;
    }

    return (bytesToRead == 0);
}

class PATListener
{
public:
    PATListener()
    : _handle(nullptr)
    {}

    bool Setup(dvbpsi_pat_callback patCallback, dvbpsi_message_cb messageCallback, dvbpsi_msg_level level)
    {
        if (_handle)
            dvbpsi_delete(_handle);
        _handle = dvbpsi_new(messageCallback, level);
        if (!_handle)
        {
            cerr << "Cannot initialize DVBPSI for PAT" << endl;
            return false;
        }
        else
            cout << "DVBPSI for PAT initialized" << endl;
        if (!dvbpsi_pat_attach(_handle, patCallback, this))
        {
            cerr << "Failed to attach PAT handler" << endl;
            return false;
        }
        else
            cout << "Attached PAT handler" << endl;
        return true;
    }
    void Cleanup()
    {
        if (_handle)
        {
            dvbpsi_pat_detach(_handle);
            cout << "Detached PAT handler" << endl;
            dvbpsi_delete(_handle);
            cout << "DVBPSI for PAT deinitialized" << endl;
        }
    }
    void PushData(uint8_t * data)
    {
        dvbpsi_packet_push(_handle, data);
    }

private:
    dvbpsi_t * _handle;
};

class NITListener
{
public:
    NITListener()
    : _handle(nullptr)
    {}

    bool Setup(dvbpsi_demux_new_cb_t demuxCallback, dvbpsi_message_cb messageCallback, dvbpsi_msg_level level)
    {
        if (_handle)
            dvbpsi_delete(_handle);
        _handle = dvbpsi_new(messageCallback, DVBPSI_MSG_DEBUG);
        if (!_handle)
        {
            cerr << "Cannot initialize DVBPSI for NIT" << endl;
            return false;
        }
        else
            cout << "DVBPSI for NIT initialized" << endl;
        if (!dvbpsi_AttachDemux(_handle, demuxCallback, this))
        {
            cerr << "Failed to attach Demux handler" << endl;
            return false;
        }
        else
            cout << "Attached Demux handler" << endl;
        return true;
    }
    bool AttachNITHandler(uint8_t table, uint8_t program, dvbpsi_nit_callback callback, void * callbackData)
    {
        return dvbpsi_nit_attach(_handle, table, program, callback, callbackData);
    }
    void Cleanup()
    {
        if (_handle)
        {
            // We need to detach all attached handlers
//            dvbpsi_nit_detach(_handle, uint8_t(SubTable::EventInformationOtherTS), 0);
//            cout << "Detached NIT handler (other TS)" << endl;
            dvbpsi_DetachDemux(_handle);
            cout << "Detached Demux handler" << endl;
            dvbpsi_delete(_handle);
            cout << "DVBPSI for NIT deinitialized" << endl;
        }
    }
    void PushData(uint8_t * data)
    {
        dvbpsi_packet_push(_handle, data);
    }

private:
    dvbpsi_t * _handle;
};

class EITListener
{
public:
    EITListener()
        : _handle(nullptr)
    {}

    bool Setup(dvbpsi_demux_new_cb_t demuxCallback, dvbpsi_message_cb messageCallback, dvbpsi_msg_level level)
    {
        if (_handle)
            dvbpsi_delete(_handle);
        _handle = dvbpsi_new(messageCallback, DVBPSI_MSG_DEBUG);
        if (!_handle)
        {
            cerr << "Cannot initialize DVBPSI for EIT" << endl;
            return false;
        }
        else
            cout << "DVBPSI for EIT initialized" << endl;
        if (!dvbpsi_AttachDemux(_handle, demuxCallback, this))
        {
            cerr << "Failed to attach Demux handler" << endl;
            return false;
        }
        else
            cout << "Attached Demux handler" << endl;
        return true;
    }
    bool AttachEITHandler(uint8_t table, uint8_t program, dvbpsi_eit_callback callback, void * callbackData)
    {
        return dvbpsi_eit_attach(_handle, table, program, callback, callbackData);
    }
    void Cleanup()
    {
        if (_handle)
        {
            // We need to detach all attached handlers
//            dvbpsi_eit_detach(_handle, uint8_t(SubTable::EventInformationOtherTS), 0);
//            cout << "Detached EIT handler (other TS)" << endl;
            dvbpsi_DetachDemux(_handle);
            cout << "Detached Demux handler" << endl;
            dvbpsi_delete(_handle);
            cout << "DVBPSI for EIT deinitialized" << endl;
        }
    }
    void PushData(uint8_t * data)
    {
        dvbpsi_packet_push(_handle, data);
    }

private:
    dvbpsi_t * _handle;
};

class TransportStreamParser
{
public:
    TransportStreamParser(int fileHandle)
        : _listenerPAT()
        , _listenerNIT()
        , _listenerEIT()
        , _reader(fileHandle)
    {}
    ~TransportStreamParser() {}

    void DumpPAT(dvbpsi_pat_t * pat);
    void DumpNIT(dvbpsi_nit_t * nit);
    void DumpEIT(dvbpsi_eit_t * eit);

    bool Setup();
    void Process();
    void Cleanup();

private:

    static void MessageCallback(dvbpsi_t * handle, const dvbpsi_msg_level_t level, const char * msg);
    static void PATCallback(void * callbackData, dvbpsi_pat_t * pat);
    static void NITCallback(void * callbackData, dvbpsi_nit_t * eit);
    static void EITCallback(void * callbackData, dvbpsi_eit_t * eit);
    static void DemuxCallback(dvbpsi_t * p_dvbpsi,  /*!< pointer to dvbpsi handle */
                              uint8_t  i_table_id, /*!< table id to attach */
                              uint16_t i_extension,/*!< table extention to attach */
                              void *  callbackData); /*!< pointer to callback data */

    PATListener _listenerPAT;
    NITListener _listenerNIT;
    EITListener _listenerEIT;
    TransportStreamReader _reader;
};

string PrintValue(uint8_t value)
{
    ostringstream stream;
    stream << dec << setw(3) << int(value) << " (0x" << hex << setw(2) << setfill('0') << int(value) << ")";
    return stream.str();
}
string PrintValue(uint16_t value)
{
    ostringstream stream;
    stream << dec << setw(5) << value << " (0x" << hex << setw(4) << setfill('0') << int(value) << ")";
    return stream.str();
}
string PrintValue(void * value)
{
    ostringstream stream;
    stream << value;
    return stream.str();
}

string GetString(char * data, size_t length, size_t & offset)
{
    string result = string(data + offset, length);
    offset += length;
    return result;
}

string GetStringWithCharSet(char * data, size_t length, size_t & offset)
{
    uint8_t charSet = static_cast<uint8_t>(data[offset]);
    offset++;
    string result = GetString(data, length - 1, offset);
    ostringstream stream;
    stream << result << " (" << int(charSet) << ")";
    return stream.str();
}

string GetStringWithCharSetAndLength(char * data, size_t & offset)
{
    size_t length = static_cast<uint8_t>(data[offset]);
    offset++;
    return GetStringWithCharSet(data, length, offset);
}

string PrintDescriptor(dvbpsi_descriptor_t *descriptor)
{
    ostringstream stream;
    stream << "Descriptor" << endl
           << "  Tag                 " << PrintValue(descriptor->i_tag) << endl
           << "  Length              " << PrintValue(descriptor->i_length) << endl
           << "  Data                " << PrintValue(descriptor->p_data) << endl
           << "  Decoded             " << PrintValue(descriptor->p_decoded) << endl;
    switch (DescriptorTag(descriptor->i_tag))
    {
    case DescriptorTag::ShortEventDescriptor:
        {
            char * data = reinterpret_cast<char *>(descriptor->p_data);
            size_t offset = 0;
            string languageCodeISO639 = GetString(data, 3, offset);
            string eventName = GetStringWithCharSetAndLength(data, offset);
            string text = GetStringWithCharSetAndLength(data, offset);
            stream << languageCodeISO639 << " " << eventName << ": " << text;
        }
        break;
    case DescriptorTag::ExtendedEventDescriptor:
        {
            char *data = reinterpret_cast<char *>(descriptor->p_data);
            size_t offset = 0;
            uint8_t descriptorInfo = static_cast<uint8_t>(data[offset]);
            offset++;
            uint8_t descriptorNumber = descriptorInfo >> 4;
            uint8_t descriptorLast = static_cast<uint8_t>(descriptorInfo & 0x0F);
            string languageCodeISO639 = GetString(data, 3, offset);
            size_t itemLength = static_cast<size_t >(data[offset]);
            offset++;
            stream << PrintValue(descriptorNumber) << "-" << PrintValue(descriptorLast) << " " << languageCodeISO639 << " ";
            size_t itemsBaseOffset = offset;
            while (offset < itemsBaseOffset + itemLength)
            {
                string itemDescription = GetStringWithCharSetAndLength(data, offset);
                string item = GetStringWithCharSetAndLength(data, offset);
                stream << itemDescription << ":" << item << endl;
            }
            string text = GetStringWithCharSetAndLength(data, offset);
            stream << ": " << text;
        }
        break;
    case DescriptorTag::ComponentDescriptor:
        {
            char *data = reinterpret_cast<char *>(descriptor->p_data);
            size_t offset = 0;
            uint8_t streamContentInfo = static_cast<uint8_t>(data[offset]);
            offset++;
            uint8_t componentType =  static_cast<uint8_t>(data[offset]);
            offset++;
            uint8_t componentTag =  static_cast<uint8_t>(data[offset]);
            offset++;
            uint8_t streamContentExt = streamContentInfo >> 4;
            uint8_t streamContent = static_cast<uint8_t>(streamContentInfo & 0x0F);
            string languageCodeISO639 = GetString(data, 3, offset);
            string text = GetStringWithCharSet(data, descriptor->i_length - offset, offset);

            stream << "Content: ";
            switch (static_cast<StreamContentType>(streamContent))
            {
            case StreamContentType ::MPEG2Video:
                stream << "MPEG2 Video";
                break;
            case StreamContentType ::MPEGAudio:
                stream << "MPEG Audio";
                break;
            default:
                break;
            }
            stream << " [" << PrintValue(streamContent) << ":" << PrintValue(streamContentExt) << "] "
                   << "Component " << PrintValue(componentType) << ":" << PrintValue(componentTag) << " "
                   << languageCodeISO639 << ": " << text;
        }
        break;
    default:
        break;
    }
    return stream.str();
}

void TransportStreamParser::DumpPAT(dvbpsi_pat_t * pat)
{
    cout << endl << "New PAT" << endl
         << "  Transport Stream ID : " << PrintValue(pat->i_ts_id) << endl
         << "  Version number      : " << PrintValue(pat->i_version) << endl
         << "    | program_number @ [NIT|PMT]_PID" << endl;

    dvbpsi_pat_program_t * program = pat->p_first_program;
    while (program)
    {
        cout << "    | " << dec << setw(14) << program->i_number
             << " @ " << PrintValue(program->i_pid) << endl;
        program = program->p_next;
    }
    cout << "  active              : " << pat->b_current_next << endl;
}

void TransportStreamParser::DumpNIT(dvbpsi_nit_t * nit)
{
    cout << endl << "New NIT" << endl
         << "  Network ID          : " << PrintValue(nit->i_network_id) << endl
         << "  Version number      : " << PrintValue(nit->i_version) << endl
         << "  Table ID            : " << PrintValue(nit->i_table_id) << endl
         << "  Sub table ID        : " << PrintValue(nit->i_extension) << endl
         << "    | program_number @ [NIT|PMT]_PID" << endl;
    dvbpsi_descriptor_t * descriptor = nit->p_first_descriptor;
    while (descriptor)
    {
        cout << PrintDescriptor(descriptor) << endl;
        descriptor = descriptor->p_next;
    }
    dvbpsi_nit_ts_t * ts = nit->p_first_ts;
    while (ts)
    {
        cout << "Transport stream ID " << PrintValue(ts->i_ts_id)
             << "Orig Network ID     " << PrintValue(ts->i_orig_network_id);
        dvbpsi_descriptor_t * descriptor = ts->p_first_descriptor;
        while (descriptor)
        {
            cout << PrintDescriptor(descriptor) << endl;
            descriptor = descriptor->p_next;
        }
        ts = ts->p_next;
    }
    cout << "  active              : " << nit->b_current_next << endl;
}

// si_time - convert DVB-SI time to seconds since 00:00
int si_time(uint32_t time)
{
    int hour, min, sec;
    hour = (time >> 16) & 0xff;
    hour = (hour >> 4) * 10 + (hour & 0x0f);
    min  = (time >> 8) & 0xff;
    min  = (min >> 4) * 10 + (min & 0x0f);
    sec  = time  & 0xff;
    sec  = (sec >> 4) * 10 + (sec & 0x0f);

    return ((hour * 60) + min) * 60 + sec;
}

// si_date - convert DVB-SI date to unix time (seconds since 1970-01-01)
time_t si_date(uint64_t date)
{
    time_t epoch;

    epoch = (date >> 24) & 0xffff;	/* modified julian date */
    if (epoch < 40587) return 0;		/* mjd-40587 = days since 1970-01-01 */
    epoch = (epoch-40587) * 24*60*60 + si_time((uint32_t)date & 0xffffff);

    return epoch;
}

string PrintTime(time_t time)
{
    struct tm tm;

    localtime_r(&time, &tm);
    ostringstream stream;
    stream << dec << setw(2) << setfill('0') << tm.tm_mday << "-"
           << dec << setw(2) << setfill('0') << tm.tm_mon + 1 << "-"
           << dec << setw(4) << setfill('0') << tm.tm_year + 1900 << " "
           << dec << setw(2) << setfill('0') << tm.tm_hour << ":"
           << dec << setw(2) << setfill('0') << tm.tm_min << ":"
           << dec << setw(2) << setfill('0') << tm.tm_sec;

    return stream.str();
}

void TransportStreamParser::DumpEIT(dvbpsi_eit_t * eit)
{
    dvbpsi_eit_event_t * event = eit->p_first_event;
    cout << endl << "New EIT" << endl
         << "  Transport stream ID : " << PrintValue(eit->i_ts_id) << endl
         << "  Network ID          : " << PrintValue(eit->i_network_id) << endl
         << "  Version number      : " << PrintValue(eit->i_version) << endl
         << "  Table ID            : " << PrintValue(eit->i_table_id) << endl
         << "  Last Table ID       : " << PrintValue(eit->i_last_table_id) << endl
         << "  Sub Table (Program) : " << PrintValue(eit->i_extension) << endl
         << "  Last Section Number : " << PrintValue(eit->i_segment_last_section_number) << endl;

    while (event)
    {
        time_t start = si_date(event->i_start_time);
        time_t end = start + si_time(event->i_duration);
        // Running status
        // Value    Meaning
        // 0        undefined
        // 1        not running
        // 2        starts in a few seconds (e.g. for video recording)
        // 3        pausing
        // 4        running
        // 5        service off-air
        // 6 to 7   reserved for future use

        cout << endl << "Event" << endl
             << "  ID             : " << PrintValue(event->i_event_id) << endl
             << "  Start          : " << PrintTime(start) << endl
             << "  End            : " << PrintTime(end) << endl
             << "  Running Status : " << PrintValue(event->i_running_status) << endl
             << "  FTA            : " << (event->b_free_ca ? "Y" : "N") << endl
             << "  NVOD           : " << (event->b_nvod ? "Y" : "N") << endl;
        dvbpsi_descriptor_t * descriptor = event->p_first_descriptor;
        while (descriptor)
        {
            cout << PrintDescriptor(descriptor) << endl;
            descriptor = descriptor->p_next;
        }
        event = event->p_next;
    }
    cout << "  active              : " << eit->b_current_next << endl;
}

void TransportStreamParser::MessageCallback(dvbpsi_t * handle,
                                            const dvbpsi_msg_level_t level,
                                            const char * msg)
{
    switch(level)
    {
        case DVBPSI_MSG_ERROR: cerr << "Error: "; break;
        case DVBPSI_MSG_WARN:  cerr << "Warning: "; break;
        case DVBPSI_MSG_DEBUG: cerr << "Debug: "; break;
        default: /* do nothing */
            return;
    }
    cerr << msg << endl;
}

void TransportStreamParser::PATCallback(void * callbackData, dvbpsi_pat_t * pat)
{
    TransportStreamParser * pThis = reinterpret_cast<TransportStreamParser *>(callbackData);
    pThis->DumpPAT(pat);

    dvbpsi_pat_program_t * program = pat->p_first_program;
    while (program)
    {
        if (!pThis->_listenerEIT.AttachEITHandler(uint8_t(SubTable::EventInformationActualTS), program->i_number, EITCallback, pThis))
        {
            cerr << "Failed to attach EIT handler (current, actual TS, for program " << program->i_number << ")" << endl;
        }
        else
            cout << "Attached EIT handler (current, actual TS, for program " << program->i_number << ")" << endl;

        if (!pThis->_listenerEIT.AttachEITHandler(uint8_t(SubTable::EventInformationActualTSNext), program->i_number, EITCallback, pThis))
        {
            cerr << "Failed to attach EIT handler (future, actual TS, for program " << program->i_number << ")" << endl;
        }
        else
            cout << "Attached EIT handler (future, actual TS, for program " << program->i_number << ")" << endl;

        if (!pThis->_listenerNIT.AttachNITHandler(uint8_t(SubTable::NetworkInformationActual), 40984, NITCallback, pThis))
        {
            cerr << "Failed to attach NIT handler (actual TS, for program " << program->i_number << ")" << endl;
        }
        else
            cout << "Attached NIT handler (actual TS, for program " << program->i_number << ")" << endl;

        if (!pThis->_listenerNIT.AttachNITHandler(uint8_t(SubTable::NetworkInformationOther), 40984, NITCallback, pThis))
        {
            cerr << "Failed to attach NIT handler (other TS, for program " << program->i_number << ")" << endl;
        }
        else
            cout << "Attached NIT handler (other TS, for program " << program->i_number << ")" << endl;

        program = program->p_next;
    }

    dvbpsi_pat_delete(pat);
}

void TransportStreamParser::NITCallback(void * callbackData, dvbpsi_nit_t * nit)
{
    TransportStreamParser * pThis = reinterpret_cast<TransportStreamParser *>(callbackData);
    pThis->DumpNIT(nit);
    dvbpsi_nit_delete(nit);
}

void TransportStreamParser::EITCallback(void * callbackData, dvbpsi_eit_t * eit)
{
    TransportStreamParser * pThis = reinterpret_cast<TransportStreamParser *>(callbackData);
    pThis->DumpEIT(eit);
    dvbpsi_eit_delete(eit);
}

void TransportStreamParser::DemuxCallback(dvbpsi_t * p_dvbpsi,  /*!< pointer to dvbpsi handle */
                                          uint8_t  i_table_id, /*!< table id to attach */
                                          uint16_t i_extension,/*!< table extention to attach */
                                          void *  callbackData) /*!< pointer to callback data */
{
    TransportStreamParser * pThis = reinterpret_cast<TransportStreamParser *>(callbackData);
    cout << endl << "New Demux" << endl
        << "  Table ID            : " << PrintValue(i_table_id) << endl
        << "  Sub table ID        : " << PrintValue(i_extension) << endl;
}

bool TransportStreamParser::Setup()
{
    if (!_listenerPAT.Setup(PATCallback, MessageCallback, DVBPSI_MSG_DEBUG))
        return false;
    if (!_listenerEIT.Setup(DemuxCallback, MessageCallback, DVBPSI_MSG_DEBUG))
        return false;
    if (!_listenerNIT.Setup(DemuxCallback, MessageCallback, DVBPSI_MSG_DEBUG))
        return false;

    return true;
}

void TransportStreamParser::Process()
{
    size_t packetCounter = 0;
    uint8_t data[TransportStreamReader::PacketSize];
    //cout << "Read packet " << packetCounter++ << endl;
    bool ok = _reader.ReadPacket(data);

    while (ok)
    {
        uint16_t pid = ((uint16_t)(data[1] & 0x1f) << 8) + data[2];
        switch (static_cast<PID>(pid))
        {
        case PID::PAT:
            _listenerPAT.PushData(data);
            break;
        case PID::NIT:
            _listenerNIT.PushData(data);
            break;
        case PID::EIT:
            _listenerEIT.PushData(data);
            break;
        default:
            //cout << "Unsupported packet PID: " << PrintValue(pid) << endl;
            break;
        }
        //cout << "Read packet " << packetCounter++ << endl;
        ok = _reader.ReadPacket(data);
    }
}

void TransportStreamParser::Cleanup()
{
    _listenerPAT.Cleanup();
    _listenerNIT.Cleanup();
    _listenerEIT.Cleanup();
}

int main(int argc, char * argv[])
{
    int fileHandle = open(argv[1], 0);
    if (fileHandle < 0)
        return 1;

    TransportStreamParser parser(fileHandle);
    parser.Setup();
    parser.Process();
    parser.Cleanup();

    close(fileHandle);

    return 0;
}