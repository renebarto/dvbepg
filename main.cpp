#include <iostream>
#include <sstream>
#include <iomanip>
#include <fcntl.h>
#include <zconf.h>
#include <dvbpsi/dvbpsi.h>
#include <dvbpsi/descriptor.h>
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
    ProgramAssociation = 0x0000,
    ConditionalAccess = 0x0001,
    ProgramMap = 0x0002,
    TSDescription = 0x0003,
    NetworkInformationActual = 0x0040,
    NetworkInformationOther = 0x0041,
    ServiceDescriptionActualTS = 0x0042,
    ServiceDescriptionOtherTS = 0x0046,
    BouquetAssociation = 0x004A,
    UpdateNotificationTable = 0x004B,
    EventInformationActualTS = 0x004E,
    EventInformationOtherTS = 0x004F,
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

class TransportStreamParser
{
public:
    TransportStreamParser(int fileHandle)
        : _handlePAT(nullptr)
        , _handleNIT(nullptr)
        , _handleEIT(nullptr)
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

    dvbpsi_t * _handlePAT;
    dvbpsi_t * _handleNIT;
    dvbpsi_t * _handleEIT;
    TransportStreamReader _reader;
};

string DumpValue(uint8_t value)
{
    ostringstream stream;
    stream << dec << setw(3) << int(value) << " (0x" << hex << setw(2) << setfill('0') << int(value) << ")";
    return stream.str();
}
string DumpValue(uint16_t value)
{
    ostringstream stream;
    stream << dec << setw(5) << value << " (0x" << hex << setw(4) << setfill('0') << int(value) << ")";
    return stream.str();
}
string DumpValue(void * value)
{
    ostringstream stream;
    stream << value;
    return stream.str();
}
string DumpDescriptor(dvbpsi_descriptor_t * descriptor)
{
    ostringstream stream;
    stream << "Tag                 " << DumpValue(descriptor->i_tag)
           << "Length              " << DumpValue(descriptor->i_length)
           << "Data                " << DumpValue(descriptor->p_data)
           << "Decoded             " << DumpValue(descriptor->p_decoded);
    return stream.str();
}

void TransportStreamParser::DumpPAT(dvbpsi_pat_t * pat)
{
    cout << endl << "New PAT" << endl
         << "  Transport Stream ID : " << DumpValue(pat->i_ts_id) << endl
         << "  Version number      : " << DumpValue(pat->i_version) << endl
         << "    | program_number @ [NIT|PMT]_PID" << endl;

    dvbpsi_pat_program_t * program = pat->p_first_program;
    while (program)
    {
        cout << "    | " << dec << setw(14) << program->i_number
             << " @ " << DumpValue(program->i_pid) << endl;
        program = program->p_next;
    }
    cout << "  active              : " << pat->b_current_next << endl;
}

void TransportStreamParser::DumpNIT(dvbpsi_nit_t * nit)
{
    cout << endl << "New NIT" << endl
         << "  Network ID          : " << DumpValue(nit->i_network_id) << endl
         << "  Version number      : " << DumpValue(nit->i_version) << endl
         << "  Table ID            : " << DumpValue(nit->i_table_id) << endl
         << "  Sub table ID        : " << DumpValue(nit->i_extension) << endl
         << "    | program_number @ [NIT|PMT]_PID" << endl;
    dvbpsi_descriptor_t * descriptor = nit->p_first_descriptor;
    while (descriptor)
    {
        cout << DumpDescriptor(descriptor) << endl;
        descriptor = descriptor->p_next;
    }
    dvbpsi_nit_ts_t * ts = nit->p_first_ts;
    while (ts)
    {
        cout << "Transport stream ID " << DumpValue(ts->i_ts_id)
             << "Orig Network ID     " << DumpValue(ts->i_orig_network_id);
        dvbpsi_descriptor_t * descriptor = ts->p_first_descriptor;
        while (descriptor)
        {
            cout << DumpDescriptor(descriptor) << endl;
            descriptor = descriptor->p_next;
        }
        ts = ts->p_next;
    }
    cout << "  active              : " << nit->b_current_next << endl;
}

void TransportStreamParser::DumpEIT(dvbpsi_eit_t * eit)
{
    dvbpsi_eit_event_t * event = eit->p_first_event;
    cout << endl << "New EIT" << endl
         << "  transport_stream_id : " << DumpValue(eit->i_ts_id) << endl
         << "  version_number      : " << DumpValue(eit->i_version) << endl
         << "  table               : " << DumpValue(eit->i_table_id) << endl
         << "  version_number      : " << DumpValue(eit->i_extension) << endl;

    while (event)
    {
        cout << "FTA  " << event->b_free_ca
             << "NVOD " << event->b_nvod
             << "ID   " << event->i_event_id << endl;
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

bool TransportStreamParser::Setup()
{
    if (_handlePAT)
        dvbpsi_delete(_handlePAT);
    _handlePAT = dvbpsi_new(MessageCallback, DVBPSI_MSG_DEBUG);
    if (!_handlePAT)
    {
        cerr << "Cannot initialize DVBPSI for PAT" << endl;
        return false;
    }
    else
        cout << "DVBPSI for PAT initialized" << endl;
    if (!dvbpsi_pat_attach(_handlePAT, PATCallback, this))
    {
        cerr << "Failed to attach PAT handler" << endl;
        return false;
    }
    else
        cout << "Attached PAT handler" << endl;
    if (_handleEIT)
        dvbpsi_delete(_handleEIT);
    _handleEIT = dvbpsi_new(MessageCallback, DVBPSI_MSG_DEBUG);
    if (!_handleEIT)
    {
        cerr << "Cannot initialize DVBPSI for EIT" << endl;
        return false;
    }
    else
        cout << "DVBPSI for EIT initialized" << endl;
    if (!dvbpsi_eit_attach(_handlePAT, uint8_t(SubTable::EventInformationActualTS), 0, EITCallback, this))
//        cout << "Attached EIT handler" << endl;
    return true;
}

void TransportStreamParser::Process()
{
    size_t packetCounter = 0;
    uint8_t data[TransportStreamReader::PacketSize];
    cout << "Read packet " << packetCounter++ << endl;
    bool ok = _reader.ReadPacket(data);

    while (ok)
    {
        uint16_t pid = ((uint16_t)(data[1] & 0x1f) << 8) + data[2];
        switch (static_cast<PID>(pid))
        {
        case PID::PAT:
            dvbpsi_packet_push(_handlePAT, data);
            break;
        case PID::NIT:
            //dvbpsi_packet_push(_handleNIT, data);
            break;
        case PID::EIT:
            dvbpsi_packet_push(_handleEIT, data);
            break;
        default:
            cout << "Unsupported packet PID: " << DumpValue(pid) << endl;
        }
        cout << "Read packet " << packetCounter++ << endl;
        ok = _reader.ReadPacket(data);
    }
}

void TransportStreamParser::Cleanup()
{
    if (_handlePAT)
    {
        dvbpsi_pat_detach(_handlePAT);
        cout << "Detached PAT handler" << endl;
        dvbpsi_delete(_handlePAT);
        cout << "DVBPSI for PAT deinitialized" << endl;
    }
    if (_handleEIT)
    {
        dvbpsi_eit_detach(_handleEIT, uint8_t(SubTable::EventInformationActualTS), 0);
        cout << "Detached EIT handler" << endl;
        dvbpsi_delete(_handleEIT);
        cout << "DVBPSI for EIT deinitialized" << endl;
    }
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