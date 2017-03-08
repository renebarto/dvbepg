#include <iostream>
#include <iomanip>
#include <fcntl.h>
#include <zconf.h>
#include <dvbpsi/dvbpsi.h>
#include <dvbpsi/descriptor.h>
#include <dvbpsi/eit.h>
#include <dvbpsi/pat.h>

using namespace std;

/*****************************************************************************
 * ReadPacket
 *****************************************************************************/
static bool ReadPacket(int i_fd, uint8_t* p_dst)
{
    int i = 187;
    int i_rc = 1;

    p_dst[0] = 0;

    while((p_dst[0] != 0x47) && (i_rc > 0))
    {
        i_rc = read(i_fd, p_dst, 1);
    }

    while((i != 0) && (i_rc > 0))
    {
        i_rc = read(i_fd, p_dst + 188 - i, i);
        if(i_rc >= 0)
            i -= i_rc;
    }

    return (i == 0) ? true : false;
}


/*****************************************************************************
 * DumpPAT
 *****************************************************************************/
static void DumpPAT(void* p_zero, dvbpsi_pat_t* p_pat)
{
    dvbpsi_pat_program_t* p_program = p_pat->p_first_program;
    printf(  "\n");
    printf(  "New PAT\n");
    printf(  "  transport_stream_id : %d\n", p_pat->i_ts_id);
    printf(  "  version_number      : %d\n", p_pat->i_version);
    printf(  "    | program_number @ [NIT|PMT]_PID\n");
    while(p_program)
    {
        printf("    | %14d @ 0x%x (%d)\n",
               p_program->i_number, p_program->i_pid, p_program->i_pid);
        p_program = p_program->p_next;
    }
    printf(  "  active              : %d\n", p_pat->b_current_next);
    dvbpsi_pat_delete(p_pat);
}


void MessageCallback(dvbpsi_t *handle,
                     const dvbpsi_msg_level_t level,
                     const char* msg)
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

void PATCallback(void* p_cb_data, dvbpsi_pat_t* p_new_pat)
{
    cout << "TS ID 0x" << hex << setw(4) << setfill('0') << p_new_pat->i_ts_id
         << " Version 0x" << hex << setw(2) << setfill('0') << int(p_new_pat->i_version)
         << " Programs:" << endl;
    dvbpsi_pat_program_t * program = p_new_pat->p_first_program;
    while (program)
    {
        cout << " PID " << dec << program->i_pid << " (" << hex << setw(4) << setfill('0') << program->i_pid << ")"
        << " Number " << dec << program->i_number << " (" << hex << setw(4) << setfill('0') << program->i_number << ")" << endl;
        program = program->p_next;
    }

}

void EITCallback(void* p_cb_data, dvbpsi_eit_t* p_new_eit)
{
}

void Setup(dvbpsi_t * handle)
{
    if (!dvbpsi_pat_attach(handle, PATCallback, nullptr))
        cout << "Failed to attach PAT handler" << endl;
    else
        cout << "Attached PAT handler" << endl;
//    if (!dvbpsi_eit_attach(handle, 0x4E, 0, EITCallback, nullptr))
//        cout << "Attached EIT handler" << endl;

}

void Process(int fileHandle, dvbpsi_t * handle)
{
    uint8_t data[188];
    bool b_ok = ReadPacket(fileHandle, data);

    while(b_ok)
    {
        uint16_t i_pid = ((uint16_t)(data[1] & 0x1f) << 8) + data[2];
        if(i_pid == 0x0)
            dvbpsi_packet_push(handle, data);
        b_ok = ReadPacket(fileHandle, data);
    }
}

void Cleanup(dvbpsi_t * handle)
{
//    dvbpsi_eit_detach(handle, 0x4E, 0);
    dvbpsi_pat_detach(handle);
}

int main(int argc, char * argv[]) {
    int fileHandle = open(argv[1], 0);
    if (fileHandle < 0)
        return 1;

    dvbpsi_t * handle = dvbpsi_new(MessageCallback, DVBPSI_MSG_DEBUG);
    cout << "DVBPSI initialized" << endl;

    Setup(handle);
    Process(fileHandle, handle);
    Cleanup(handle);

    dvbpsi_delete(handle);
    cout << "DVBPSI deinitialized" << endl;

    close(fileHandle);

    return 0;
}