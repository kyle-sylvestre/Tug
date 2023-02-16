/* SPDX-License-Identifier: 0BSD */
/* Copyright 2019 Alexander Kozhevnikov <mentalisttraceur@gmail.com> */

#ifndef ERRNONAME_C
#define ERRNONAME_C

#include <errno.h>

char const * errnoname(int errno_)
{
    switch(errno_)
    {
        case 0: return 0;
    #ifdef E2BIG
        case E2BIG: return "E2BIG";
    #endif
    #ifdef EACCES
        case EACCES: return "EACCES";
    #endif
    #ifdef EACTIVE
        case EACTIVE: return "EACTIVE";
    #endif
    #ifdef EADDRINUSE
        case EADDRINUSE: return "EADDRINUSE";
    #endif
    #ifdef EADDRNOTAVAIL
        case EADDRNOTAVAIL: return "EADDRNOTAVAIL";
    #endif
    #ifdef EADI
        case EADI: return "EADI";
    #endif
    #ifdef EADV
        case EADV: return "EADV";
    #endif
    #ifdef EAFNOSUPPORT
        case EAFNOSUPPORT: return "EAFNOSUPPORT";
    #endif
    #ifdef EAGAIN
        case EAGAIN: return "EAGAIN";
    #endif
    #ifdef EAIO
        case EAIO: return "EAIO";
    #endif
    #ifdef EAI_AGAIN
        case EAI_AGAIN: return "EAI_AGAIN";
    #endif
    #ifdef EAI_BADFLAGS
        case EAI_BADFLAGS: return "EAI_BADFLAGS";
    #endif
    #ifdef EAI_FAIL
        case EAI_FAIL: return "EAI_FAIL";
    #endif
    #ifdef EAI_FAMILY
        case EAI_FAMILY: return "EAI_FAMILY";
    #endif
    #ifdef EAI_MEMORY
        case EAI_MEMORY: return "EAI_MEMORY";
    #endif
    #ifdef EAI_NONAME
        case EAI_NONAME: return "EAI_NONAME";
    #endif
    #ifdef EAI_OVERFLOW
        case EAI_OVERFLOW: return "EAI_OVERFLOW";
    #endif
    #ifdef EAI_SERVICE
        case EAI_SERVICE: return "EAI_SERVICE";
    #endif
    #ifdef EAI_SOCKTYPE
        case EAI_SOCKTYPE: return "EAI_SOCKTYPE";
    #endif
    #ifdef EALIGN
        case EALIGN: return "EALIGN";
    #endif
    #ifdef EALREADY
        case EALREADY: return "EALREADY";
    #endif
    #ifdef EASYNC
        case EASYNC: return "EASYNC";
    #endif
    #ifdef EAUTH
        case EAUTH: return "EAUTH";
    #endif
    #ifdef EBACKGROUND
        case EBACKGROUND: return "EBACKGROUND";
    #endif
    #ifdef EBADARCH
        case EBADARCH: return "EBADARCH";
    #endif
    #ifdef EBADCALL
        case EBADCALL: return "EBADCALL";
    #endif
    #ifdef EBADCOOKIE
        case EBADCOOKIE: return "EBADCOOKIE";
    #endif
    #ifdef EBADCPU
        case EBADCPU: return "EBADCPU";
    #endif
    #ifdef EBADE
        case EBADE: return "EBADE";
    #endif
    #ifdef EBADEPT
        case EBADEPT: return "EBADEPT";
    #endif
    #ifdef EBADEXEC
        case EBADEXEC: return "EBADEXEC";
    #endif
    #ifdef EBADF
        case EBADF: return "EBADF";
    #endif
    #ifdef EBADFD
        case EBADFD: return "EBADFD";
    #endif
    #ifdef EBADFSYS
        case EBADFSYS: return "EBADFSYS";
    #endif
    #ifdef EBADHANDLE
        case EBADHANDLE: return "EBADHANDLE";
    #endif
    #ifdef EBADMACHO
        case EBADMACHO: return "EBADMACHO";
    #endif
    #ifdef EBADMODE
        case EBADMODE: return "EBADMODE";
    #endif
    #ifdef EBADMSG
        case EBADMSG: return "EBADMSG";
    #endif
    #ifdef EBADOBJ
        case EBADOBJ: return "EBADOBJ";
    #endif
    #ifdef EBADR
        case EBADR: return "EBADR";
    #endif
    #ifdef EBADREQUEST
        case EBADREQUEST: return "EBADREQUEST";
    #endif
    #ifdef EBADRPC
        case EBADRPC: return "EBADRPC";
    #endif
    #ifdef EBADRQC
        case EBADRQC: return "EBADRQC";
    #endif
    #ifdef EBADSLT
        case EBADSLT: return "EBADSLT";
    #endif
    #ifdef EBADTYPE
        case EBADTYPE: return "EBADTYPE";
    #endif
    #ifdef EBADVER
        case EBADVER: return "EBADVER";
    #endif
    #ifdef EBFONT
        case EBFONT: return "EBFONT";
    #endif
    #ifdef EBUSY
        case EBUSY: return "EBUSY";
    #endif
    #ifdef ECALLDENIED
        case ECALLDENIED: return "ECALLDENIED";
    #endif
    #ifdef ECANCEL
        case ECANCEL: return "ECANCEL";
    #endif
    #ifdef ECANCELED
        case ECANCELED: return "ECANCELED";
    #endif
    #ifdef ECANCELLED
        #if !defined(ECANCELED) || ECANCELLED != ECANCELED
        case ECANCELLED: return "ECANCELLED";
        #endif
    #endif
    #ifdef ECAPMODE
        case ECAPMODE: return "ECAPMODE";
    #endif
    #ifdef ECASECLASH
        case ECASECLASH: return "ECASECLASH";
    #endif
    #ifdef ECHILD
        case ECHILD: return "ECHILD";
    #endif
    #ifdef ECHRNG
        case ECHRNG: return "ECHRNG";
    #endif
    #ifdef ECKPT
        case ECKPT: return "ECKPT";
    #endif
    #ifdef ECKSUM
        case ECKSUM: return "ECKSUM";
    #endif
    #ifdef ECLONEME
        case ECLONEME: return "ECLONEME";
    #endif
    #ifdef ECLOSED
        case ECLOSED: return "ECLOSED";
    #endif
    #ifdef ECOMM
        case ECOMM: return "ECOMM";
    #endif
    #ifdef ECONFIG
        case ECONFIG: return "ECONFIG";
    #endif
    #ifdef ECONNABORTED
        case ECONNABORTED: return "ECONNABORTED";
    #endif
    #ifdef ECONNCLOSED
        case ECONNCLOSED: return "ECONNCLOSED";
    #endif
    #ifdef ECONNREFUSED
        case ECONNREFUSED: return "ECONNREFUSED";
    #endif
    #ifdef ECONNRESET
        case ECONNRESET: return "ECONNRESET";
    #endif
    #ifdef ECONSOLEINTERRUPT
        case ECONSOLEINTERRUPT: return "ECONSOLEINTERRUPT";
    #endif
    #ifdef ECORRUPT
        case ECORRUPT: return "ECORRUPT";
    #endif
    #ifdef ECTRLTERM
        case ECTRLTERM: return "ECTRLTERM";
    #endif
    #ifdef ECVCERORR
        case ECVCERORR: return "ECVCERORR";
    #endif
    #ifdef ECVPERORR
        case ECVPERORR: return "ECVPERORR";
    #endif
    #ifdef ED
        case ED: return "ED";
    #endif
    #ifdef EDATALESS
        case EDATALESS: return "EDATALESS";
    #endif
    #ifdef EDEADEPT
        case EDEADEPT: return "EDEADEPT";
    #endif
    #ifdef EDEADLK
        case EDEADLK: return "EDEADLK";
    #endif
    #ifdef EDEADLOCK
        #if !defined(EDEADLK) || EDEADLOCK != EDEADLK
        case EDEADLOCK: return "EDEADLOCK";
        #endif
    #endif
    #ifdef EDEADSRCDST
        case EDEADSRCDST: return "EDEADSRCDST";
    #endif
    #ifdef EDESTADDREQ
        #if !defined(EDESTADDRREQ) || EDESTADDREQ != EDESTADDRREQ
        case EDESTADDREQ: return "EDESTADDREQ";
        #endif
    #endif
    #ifdef EDESTADDRREQ
        case EDESTADDRREQ: return "EDESTADDRREQ";
    #endif
    #ifdef EDEVERR
        case EDEVERR: return "EDEVERR";
    #endif
    #ifdef EDIED
        case EDIED: return "EDIED";
    #endif
    #ifdef EDIRIOCTL
        case EDIRIOCTL: return "EDIRIOCTL";
    #endif
    #ifdef EDIRTY
        case EDIRTY: return "EDIRTY";
    #endif
    #ifdef EDIST
        case EDIST: return "EDIST";
    #endif
    #ifdef EDOM
        case EDOM: return "EDOM";
    #endif
    #ifdef EDOMAINSERVERFAILURE
        case EDOMAINSERVERFAILURE: return "EDOMAINSERVERFAILURE";
    #endif
    #ifdef EDONTREPLY
        case EDONTREPLY: return "EDONTREPLY";
    #endif
    #ifdef EDOOFUS
        case EDOOFUS: return "EDOOFUS";
    #endif
    #ifdef EDOTDOT
        case EDOTDOT: return "EDOTDOT";
    #endif
    #ifdef EDQUOT
        case EDQUOT: return "EDQUOT";
    #endif
    #ifdef EDUPBADOPCODE
        case EDUPBADOPCODE: return "EDUPBADOPCODE";
    #endif
    #ifdef EDUPFD
        case EDUPFD: return "EDUPFD";
    #endif
    #ifdef EDUPINTRANSIT
        case EDUPINTRANSIT: return "EDUPINTRANSIT";
    #endif
    #ifdef EDUPNOCONN
        case EDUPNOCONN: return "EDUPNOCONN";
    #endif
    #ifdef EDUPNODISCONN
        case EDUPNODISCONN: return "EDUPNODISCONN";
    #endif
    #ifdef EDUPNOTCNTD
        case EDUPNOTCNTD: return "EDUPNOTCNTD";
    #endif
    #ifdef EDUPNOTIDLE
        case EDUPNOTIDLE: return "EDUPNOTIDLE";
    #endif
    #ifdef EDUPNOTRUN
        case EDUPNOTRUN: return "EDUPNOTRUN";
    #endif
    #ifdef EDUPNOTWAIT
        case EDUPNOTWAIT: return "EDUPNOTWAIT";
    #endif
    #ifdef EDUPPKG
        case EDUPPKG: return "EDUPPKG";
    #endif
    #ifdef EDUPTOOMANYCPUS
        case EDUPTOOMANYCPUS: return "EDUPTOOMANYCPUS";
    #endif
    #ifdef ED_ALREADY_OPEN
        case ED_ALREADY_OPEN: return "ED_ALREADY_OPEN";
    #endif
    #ifdef ED_DEVICE_DOWN
        case ED_DEVICE_DOWN: return "ED_DEVICE_DOWN";
    #endif
    #ifdef ED_INVALID_OPERATION
        case ED_INVALID_OPERATION: return "ED_INVALID_OPERATION";
    #endif
    #ifdef ED_INVALID_RECNUM
        case ED_INVALID_RECNUM: return "ED_INVALID_RECNUM";
    #endif
    #ifdef ED_INVALID_SIZE
        case ED_INVALID_SIZE: return "ED_INVALID_SIZE";
    #endif
    #ifdef ED_IO_ERROR
        case ED_IO_ERROR: return "ED_IO_ERROR";
    #endif
    #ifdef ED_NO_MEMORY
        case ED_NO_MEMORY: return "ED_NO_MEMORY";
    #endif
    #ifdef ED_NO_SUCH_DEVICE
        case ED_NO_SUCH_DEVICE: return "ED_NO_SUCH_DEVICE";
    #endif
    #ifdef ED_READ_ONLY
        case ED_READ_ONLY: return "ED_READ_ONLY";
    #endif
    #ifdef ED_WOULD_BLOCK
        case ED_WOULD_BLOCK: return "ED_WOULD_BLOCK";
    #endif
    #ifdef EENDIAN
        case EENDIAN: return "EENDIAN";
    #endif
    #ifdef EEXIST
        case EEXIST: return "EEXIST";
    #endif
    #ifdef EFAIL
        case EFAIL: return "EFAIL";
    #endif
    #ifdef EFAULT
        case EFAULT: return "EFAULT";
    #endif
    #ifdef EFBIG
        case EFBIG: return "EFBIG";
    #endif
    #ifdef EFORMAT
        case EFORMAT: return "EFORMAT";
    #endif
    #ifdef EFPOS
        case EFPOS: return "EFPOS";
    #endif
    #ifdef EFRAGS
        case EFRAGS: return "EFRAGS";
    #endif
    #ifdef EFSCORRUPTED
        case EFSCORRUPTED: return "EFSCORRUPTED";
    #endif
    #ifdef EFTYPE
        case EFTYPE: return "EFTYPE";
    #endif
    #ifdef EGENERIC
        case EGENERIC: return "EGENERIC";
    #endif
    #ifdef EGRATUITOUS
        case EGRATUITOUS: return "EGRATUITOUS";
    #endif
    #ifdef EGREGIOUS
        case EGREGIOUS: return "EGREGIOUS";
    #endif
    #ifdef EHOSTDOWN
        case EHOSTDOWN: return "EHOSTDOWN";
    #endif
    #ifdef EHOSTNOTFOUND
        case EHOSTNOTFOUND: return "EHOSTNOTFOUND";
    #endif
    #ifdef EHOSTUNREACH
        case EHOSTUNREACH: return "EHOSTUNREACH";
    #endif
    #ifdef EHWPOISON
        case EHWPOISON: return "EHWPOISON";
    #endif
    #ifdef EIBMBADCONNECTIONMATCH
        case EIBMBADCONNECTIONMATCH: return "EIBMBADCONNECTIONMATCH";
    #endif
    #ifdef EIBMBADCONNECTIONSTATE
        case EIBMBADCONNECTIONSTATE: return "EIBMBADCONNECTIONSTATE";
    #endif
    #ifdef EIBMBADREQUESTCODE
        case EIBMBADREQUESTCODE: return "EIBMBADREQUESTCODE";
    #endif
    #ifdef EIBMBADTCPNAME
        case EIBMBADTCPNAME: return "EIBMBADTCPNAME";
    #endif
    #ifdef EIBMCALLINPROGRESS
        case EIBMCALLINPROGRESS: return "EIBMCALLINPROGRESS";
    #endif
    #ifdef EIBMCANCELLED
        case EIBMCANCELLED: return "EIBMCANCELLED";
    #endif
    #ifdef EIBMCONFLICT
        case EIBMCONFLICT: return "EIBMCONFLICT";
    #endif
    #ifdef EIBMINVDELETE
        case EIBMINVDELETE: return "EIBMINVDELETE";
    #endif
    #ifdef EIBMINVSOCKET
        case EIBMINVSOCKET: return "EIBMINVSOCKET";
    #endif
    #ifdef EIBMINVTCPCONNECTION
        case EIBMINVTCPCONNECTION: return "EIBMINVTCPCONNECTION";
    #endif
    #ifdef EIBMINVTSRBUSERDATA
        case EIBMINVTSRBUSERDATA: return "EIBMINVTSRBUSERDATA";
    #endif
    #ifdef EIBMINVUSERDATA
        case EIBMINVUSERDATA: return "EIBMINVUSERDATA";
    #endif
    #ifdef EIBMIUCVERR
        case EIBMIUCVERR: return "EIBMIUCVERR";
    #endif
    #ifdef EIBMNOACTIVETCP
        case EIBMNOACTIVETCP: return "EIBMNOACTIVETCP";
    #endif
    #ifdef EIBMSELECTEXPOST
        case EIBMSELECTEXPOST: return "EIBMSELECTEXPOST";
    #endif
    #ifdef EIBMSOCKINUSE
        case EIBMSOCKINUSE: return "EIBMSOCKINUSE";
    #endif
    #ifdef EIBMSOCKOUTOFRANGE
        case EIBMSOCKOUTOFRANGE: return "EIBMSOCKOUTOFRANGE";
    #endif
    #ifdef EIBMTCPABEND
        case EIBMTCPABEND: return "EIBMTCPABEND";
    #endif
    #ifdef EIBMTERMERROR
        case EIBMTERMERROR: return "EIBMTERMERROR";
    #endif
    #ifdef EIBMUNAUTHORIZEDCALLER
        case EIBMUNAUTHORIZEDCALLER: return "EIBMUNAUTHORIZEDCALLER";
    #endif
    #ifdef EIDRM
        case EIDRM: return "EIDRM";
    #endif
    #ifdef EIEIO
        case EIEIO: return "EIEIO";
    #endif
    #ifdef EILSEQ
        case EILSEQ: return "EILSEQ";
    #endif
    #ifdef EINIT
        case EINIT: return "EINIT";
    #endif
    #ifdef EINPROG
        #if !defined(EINPROGRESS) || EINPROG != EINPROGRESS
        case EINPROG: return "EINPROG";
        #endif
    #endif
    #ifdef EINPROGRESS
        case EINPROGRESS: return "EINPROGRESS";
    #endif
    #ifdef EINTEGRITY
        case EINTEGRITY: return "EINTEGRITY";
    #endif
    #ifdef EINTR
        case EINTR: return "EINTR";
    #endif
    #ifdef EINTRNODATA
        case EINTRNODATA: return "EINTRNODATA";
    #endif
    #ifdef EINVAL
        case EINVAL: return "EINVAL";
    #endif
    #ifdef EINVALIDCLIENTID
        case EINVALIDCLIENTID: return "EINVALIDCLIENTID";
    #endif
    #ifdef EINVALIDCOMBINATION
        case EINVALIDCOMBINATION: return "EINVALIDCOMBINATION";
    #endif
    #ifdef EINVALIDNAME
        case EINVALIDNAME: return "EINVALIDNAME";
    #endif
    #ifdef EINVALIDRXSOCKETCALL
        case EINVALIDRXSOCKETCALL: return "EINVALIDRXSOCKETCALL";
    #endif
    #ifdef EIO
        case EIO: return "EIO";
    #endif
    #ifdef EIOCBQUEUED
        case EIOCBQUEUED: return "EIOCBQUEUED";
    #endif
    #ifdef EIORESID
        case EIORESID: return "EIORESID";
    #endif
    #ifdef EIPADDRNOTFOUND
        case EIPADDRNOTFOUND: return "EIPADDRNOTFOUND";
    #endif
    #ifdef EIPSEC
        case EIPSEC: return "EIPSEC";
    #endif
    #ifdef EISCONN
        case EISCONN: return "EISCONN";
    #endif
    #ifdef EISDIR
        case EISDIR: return "EISDIR";
    #endif
    #ifdef EISNAM
        case EISNAM: return "EISNAM";
    #endif
    #ifdef EJUKEBOX
        case EJUKEBOX: return "EJUKEBOX";
    #endif
    #ifdef EJUSTRETURN
        case EJUSTRETURN: return "EJUSTRETURN";
    #endif
    #ifdef EKEEPLOOKING
        case EKEEPLOOKING: return "EKEEPLOOKING";
    #endif
    #ifdef EKERN_ABORTED
        case EKERN_ABORTED: return "EKERN_ABORTED";
    #endif
    #ifdef EKERN_FAILURE
        case EKERN_FAILURE: return "EKERN_FAILURE";
    #endif
    #ifdef EKERN_INTERRUPTED
        case EKERN_INTERRUPTED: return "EKERN_INTERRUPTED";
    #endif
    #ifdef EKERN_INVALID_ADDRESS
        case EKERN_INVALID_ADDRESS: return "EKERN_INVALID_ADDRESS";
    #endif
    #ifdef EKERN_INVALID_ARGUMENT
        case EKERN_INVALID_ARGUMENT: return "EKERN_INVALID_ARGUMENT";
    #endif
    #ifdef EKERN_INVALID_CAPABILITY
        case EKERN_INVALID_CAPABILITY: return "EKERN_INVALID_CAPABILITY";
    #endif
    #ifdef EKERN_INVALID_HOST
        case EKERN_INVALID_HOST: return "EKERN_INVALID_HOST";
    #endif
    #ifdef EKERN_INVALID_NAME
        case EKERN_INVALID_NAME: return "EKERN_INVALID_NAME";
    #endif
    #ifdef EKERN_INVALID_RIGHT
        case EKERN_INVALID_RIGHT: return "EKERN_INVALID_RIGHT";
    #endif
    #ifdef EKERN_INVALID_TASK
        case EKERN_INVALID_TASK: return "EKERN_INVALID_TASK";
    #endif
    #ifdef EKERN_INVALID_VALUE
        case EKERN_INVALID_VALUE: return "EKERN_INVALID_VALUE";
    #endif
    #ifdef EKERN_MEMORY_ERROR
        case EKERN_MEMORY_ERROR: return "EKERN_MEMORY_ERROR";
    #endif
    #ifdef EKERN_MEMORY_FAILURE
        case EKERN_MEMORY_FAILURE: return "EKERN_MEMORY_FAILURE";
    #endif
    #ifdef EKERN_MEMORY_PRESENT
        case EKERN_MEMORY_PRESENT: return "EKERN_MEMORY_PRESENT";
    #endif
    #ifdef EKERN_NAME_EXISTS
        case EKERN_NAME_EXISTS: return "EKERN_NAME_EXISTS";
    #endif
    #ifdef EKERN_NOT_IN_SET
        case EKERN_NOT_IN_SET: return "EKERN_NOT_IN_SET";
    #endif
    #ifdef EKERN_NOT_RECEIVER
        case EKERN_NOT_RECEIVER: return "EKERN_NOT_RECEIVER";
    #endif
    #ifdef EKERN_NO_ACCESS
        case EKERN_NO_ACCESS: return "EKERN_NO_ACCESS";
    #endif
    #ifdef EKERN_NO_SPACE
        case EKERN_NO_SPACE: return "EKERN_NO_SPACE";
    #endif
    #ifdef EKERN_PROTECTION_FAILURE
        case EKERN_PROTECTION_FAILURE: return "EKERN_PROTECTION_FAILURE";
    #endif
    #ifdef EKERN_RESOURCE_SHORTAGE
        case EKERN_RESOURCE_SHORTAGE: return "EKERN_RESOURCE_SHORTAGE";
    #endif
    #ifdef EKERN_RIGHT_EXISTS
        case EKERN_RIGHT_EXISTS: return "EKERN_RIGHT_EXISTS";
    #endif
    #ifdef EKERN_TERMINATED
        case EKERN_TERMINATED: return "EKERN_TERMINATED";
    #endif
    #ifdef EKERN_TIMEDOUT
        case EKERN_TIMEDOUT: return "EKERN_TIMEDOUT";
    #endif
    #ifdef EKERN_UREFS_OVERFLOW
        case EKERN_UREFS_OVERFLOW: return "EKERN_UREFS_OVERFLOW";
    #endif
    #ifdef EKERN_WRITE_PROTECTION_FAILURE
        case EKERN_WRITE_PROTECTION_FAILURE: return "EKERN_WRITE_PROTECTION_FAILURE";
    #endif
    #ifdef EKEYEXPIRED
        case EKEYEXPIRED: return "EKEYEXPIRED";
    #endif
    #ifdef EKEYREJECTED
        case EKEYREJECTED: return "EKEYREJECTED";
    #endif
    #ifdef EKEYREVOKED
        case EKEYREVOKED: return "EKEYREVOKED";
    #endif
    #ifdef EL2HLT
        case EL2HLT: return "EL2HLT";
    #endif
    #ifdef EL2NSYNC
        case EL2NSYNC: return "EL2NSYNC";
    #endif
    #ifdef EL3HLT
        case EL3HLT: return "EL3HLT";
    #endif
    #ifdef EL3RST
        case EL3RST: return "EL3RST";
    #endif
    #ifdef ELBIN
        case ELBIN: return "ELBIN";
    #endif
    #ifdef ELIBACC
        case ELIBACC: return "ELIBACC";
    #endif
    #ifdef ELIBBAD
        case ELIBBAD: return "ELIBBAD";
    #endif
    #ifdef ELIBEXEC
        case ELIBEXEC: return "ELIBEXEC";
    #endif
    #ifdef ELIBMAX
        case ELIBMAX: return "ELIBMAX";
    #endif
    #ifdef ELIBSCN
        case ELIBSCN: return "ELIBSCN";
    #endif
    #ifdef ELINKED
        case ELINKED: return "ELINKED";
    #endif
    #ifdef ELNRNG
        case ELNRNG: return "ELNRNG";
    #endif
    #ifdef ELOCKED
        case ELOCKED: return "ELOCKED";
    #endif
    #ifdef ELOCKUNMAPPED
        case ELOCKUNMAPPED: return "ELOCKUNMAPPED";
    #endif
    #ifdef ELOOP
        case ELOOP: return "ELOOP";
    #endif
    #ifdef EMACH_RCV_BODY_ERROR
        case EMACH_RCV_BODY_ERROR: return "EMACH_RCV_BODY_ERROR";
    #endif
    #ifdef EMACH_RCV_HEADER_ERROR
        case EMACH_RCV_HEADER_ERROR: return "EMACH_RCV_HEADER_ERROR";
    #endif
    #ifdef EMACH_RCV_INTERRUPTED
        case EMACH_RCV_INTERRUPTED: return "EMACH_RCV_INTERRUPTED";
    #endif
    #ifdef EMACH_RCV_INVALID_DATA
        case EMACH_RCV_INVALID_DATA: return "EMACH_RCV_INVALID_DATA";
    #endif
    #ifdef EMACH_RCV_INVALID_NAME
        case EMACH_RCV_INVALID_NAME: return "EMACH_RCV_INVALID_NAME";
    #endif
    #ifdef EMACH_RCV_INVALID_NOTIFY
        case EMACH_RCV_INVALID_NOTIFY: return "EMACH_RCV_INVALID_NOTIFY";
    #endif
    #ifdef EMACH_RCV_IN_PROGRESS
        case EMACH_RCV_IN_PROGRESS: return "EMACH_RCV_IN_PROGRESS";
    #endif
    #ifdef EMACH_RCV_IN_SET
        case EMACH_RCV_IN_SET: return "EMACH_RCV_IN_SET";
    #endif
    #ifdef EMACH_RCV_PORT_CHANGED
        case EMACH_RCV_PORT_CHANGED: return "EMACH_RCV_PORT_CHANGED";
    #endif
    #ifdef EMACH_RCV_PORT_DIED
        case EMACH_RCV_PORT_DIED: return "EMACH_RCV_PORT_DIED";
    #endif
    #ifdef EMACH_RCV_TIMED_OUT
        case EMACH_RCV_TIMED_OUT: return "EMACH_RCV_TIMED_OUT";
    #endif
    #ifdef EMACH_RCV_TOO_LARGE
        case EMACH_RCV_TOO_LARGE: return "EMACH_RCV_TOO_LARGE";
    #endif
    #ifdef EMACH_SEND_INTERRUPTED
        case EMACH_SEND_INTERRUPTED: return "EMACH_SEND_INTERRUPTED";
    #endif
    #ifdef EMACH_SEND_INVALID_DATA
        case EMACH_SEND_INVALID_DATA: return "EMACH_SEND_INVALID_DATA";
    #endif
    #ifdef EMACH_SEND_INVALID_DEST
        case EMACH_SEND_INVALID_DEST: return "EMACH_SEND_INVALID_DEST";
    #endif
    #ifdef EMACH_SEND_INVALID_HEADER
        case EMACH_SEND_INVALID_HEADER: return "EMACH_SEND_INVALID_HEADER";
    #endif
    #ifdef EMACH_SEND_INVALID_MEMORY
        case EMACH_SEND_INVALID_MEMORY: return "EMACH_SEND_INVALID_MEMORY";
    #endif
    #ifdef EMACH_SEND_INVALID_NOTIFY
        case EMACH_SEND_INVALID_NOTIFY: return "EMACH_SEND_INVALID_NOTIFY";
    #endif
    #ifdef EMACH_SEND_INVALID_REPLY
        case EMACH_SEND_INVALID_REPLY: return "EMACH_SEND_INVALID_REPLY";
    #endif
    #ifdef EMACH_SEND_INVALID_RIGHT
        case EMACH_SEND_INVALID_RIGHT: return "EMACH_SEND_INVALID_RIGHT";
    #endif
    #ifdef EMACH_SEND_INVALID_TYPE
        case EMACH_SEND_INVALID_TYPE: return "EMACH_SEND_INVALID_TYPE";
    #endif
    #ifdef EMACH_SEND_IN_PROGRESS
        case EMACH_SEND_IN_PROGRESS: return "EMACH_SEND_IN_PROGRESS";
    #endif
    #ifdef EMACH_SEND_MSG_TOO_SMALL
        case EMACH_SEND_MSG_TOO_SMALL: return "EMACH_SEND_MSG_TOO_SMALL";
    #endif
    #ifdef EMACH_SEND_NOTIFY_IN_PROGRESS
        case EMACH_SEND_NOTIFY_IN_PROGRESS: return "EMACH_SEND_NOTIFY_IN_PROGRESS";
    #endif
    #ifdef EMACH_SEND_NO_BUFFER
        case EMACH_SEND_NO_BUFFER: return "EMACH_SEND_NO_BUFFER";
    #endif
    #ifdef EMACH_SEND_NO_NOTIFY
        case EMACH_SEND_NO_NOTIFY: return "EMACH_SEND_NO_NOTIFY";
    #endif
    #ifdef EMACH_SEND_TIMED_OUT
        case EMACH_SEND_TIMED_OUT: return "EMACH_SEND_TIMED_OUT";
    #endif
    #ifdef EMACH_SEND_WILL_NOTIFY
        case EMACH_SEND_WILL_NOTIFY: return "EMACH_SEND_WILL_NOTIFY";
    #endif
    #ifdef EMAXSOCKETSREACHED
        case EMAXSOCKETSREACHED: return "EMAXSOCKETSREACHED";
    #endif
    #ifdef EMEDIA
        case EMEDIA: return "EMEDIA";
    #endif
    #ifdef EMEDIUMTYPE
        case EMEDIUMTYPE: return "EMEDIUMTYPE";
    #endif
    #ifdef EMFILE
        case EMFILE: return "EMFILE";
    #endif
    #ifdef EMIG_ARRAY_TOO_LARGE
        case EMIG_ARRAY_TOO_LARGE: return "EMIG_ARRAY_TOO_LARGE";
    #endif
    #ifdef EMIG_BAD_ARGUMENTS
        case EMIG_BAD_ARGUMENTS: return "EMIG_BAD_ARGUMENTS";
    #endif
    #ifdef EMIG_BAD_ID
        case EMIG_BAD_ID: return "EMIG_BAD_ID";
    #endif
    #ifdef EMIG_DESTROY_REQUEST
        case EMIG_DESTROY_REQUEST: return "EMIG_DESTROY_REQUEST";
    #endif
    #ifdef EMIG_EXCEPTION
        case EMIG_EXCEPTION: return "EMIG_EXCEPTION";
    #endif
    #ifdef EMIG_NO_REPLY
        case EMIG_NO_REPLY: return "EMIG_NO_REPLY";
    #endif
    #ifdef EMIG_REMOTE_ERROR
        case EMIG_REMOTE_ERROR: return "EMIG_REMOTE_ERROR";
    #endif
    #ifdef EMIG_REPLY_MISMATCH
        case EMIG_REPLY_MISMATCH: return "EMIG_REPLY_MISMATCH";
    #endif
    #ifdef EMIG_SERVER_DIED
        case EMIG_SERVER_DIED: return "EMIG_SERVER_DIED";
    #endif
    #ifdef EMIG_TYPE_ERROR
        case EMIG_TYPE_ERROR: return "EMIG_TYPE_ERROR";
    #endif
    #ifdef EMISSED
        case EMISSED: return "EMISSED";
    #endif
    #ifdef EMLINK
        case EMLINK: return "EMLINK";
    #endif
    #ifdef EMORE
        case EMORE: return "EMORE";
    #endif
    #ifdef EMOUNTEXIT
        case EMOUNTEXIT: return "EMOUNTEXIT";
    #endif
    #ifdef EMOVEFD
        case EMOVEFD: return "EMOVEFD";
    #endif
    #ifdef EMSGSIZE
        case EMSGSIZE: return "EMSGSIZE";
    #endif
    #ifdef EMTIMERS
        case EMTIMERS: return "EMTIMERS";
    #endif
    #ifdef EMULTIHOP
        case EMULTIHOP: return "EMULTIHOP";
    #endif
    #ifdef EMVSARMERROR
        case EMVSARMERROR: return "EMVSARMERROR";
    #endif
    #ifdef EMVSCATLG
        case EMVSCATLG: return "EMVSCATLG";
    #endif
    #ifdef EMVSCPLERROR
        case EMVSCPLERROR: return "EMVSCPLERROR";
    #endif
    #ifdef EMVSCVAF
        case EMVSCVAF: return "EMVSCVAF";
    #endif
    #ifdef EMVSDYNALC
        case EMVSDYNALC: return "EMVSDYNALC";
    #endif
    #ifdef EMVSERR
        case EMVSERR: return "EMVSERR";
    #endif
    #ifdef EMVSEXPIRE
        case EMVSEXPIRE: return "EMVSEXPIRE";
    #endif
    #ifdef EMVSINITIAL
        case EMVSINITIAL: return "EMVSINITIAL";
    #endif
    #ifdef EMVSNORTL
        case EMVSNORTL: return "EMVSNORTL";
    #endif
    #ifdef EMVSNOTUP
        case EMVSNOTUP: return "EMVSNOTUP";
    #endif
    #ifdef EMVSPARM
        case EMVSPARM: return "EMVSPARM";
    #endif
    #ifdef EMVSPASSWORD
        case EMVSPASSWORD: return "EMVSPASSWORD";
    #endif
    #ifdef EMVSPFSFILE
        case EMVSPFSFILE: return "EMVSPFSFILE";
    #endif
    #ifdef EMVSPFSPERM
        case EMVSPFSPERM: return "EMVSPFSPERM";
    #endif
    #ifdef EMVSSAF2ERR
        case EMVSSAF2ERR: return "EMVSSAF2ERR";
    #endif
    #ifdef EMVSSAFEXTRERR
        case EMVSSAFEXTRERR: return "EMVSSAFEXTRERR";
    #endif
    #ifdef EMVSWLMERROR
        case EMVSWLMERROR: return "EMVSWLMERROR";
    #endif
    #ifdef ENAMETOOLONG
        case ENAMETOOLONG: return "ENAMETOOLONG";
    #endif
    #ifdef ENAVAIL
        case ENAVAIL: return "ENAVAIL";
    #endif
    #ifdef ENEEDAUTH
        case ENEEDAUTH: return "ENEEDAUTH";
    #endif
    #ifdef ENETDOWN
        case ENETDOWN: return "ENETDOWN";
    #endif
    #ifdef ENETRESET
        case ENETRESET: return "ENETRESET";
    #endif
    #ifdef ENETUNREACH
        case ENETUNREACH: return "ENETUNREACH";
    #endif
    #ifdef ENFILE
        case ENFILE: return "ENFILE";
    #endif
    #ifdef ENFSREMOTE
        case ENFSREMOTE: return "ENFSREMOTE";
    #endif
    #ifdef ENIVALIDFILENAME
        case ENIVALIDFILENAME: return "ENIVALIDFILENAME";
    #endif
    #ifdef ENMELONG
        case ENMELONG: return "ENMELONG";
    #endif
    #ifdef ENMFILE
        case ENMFILE: return "ENMFILE";
    #endif
    #ifdef ENOACTIVE
        case ENOACTIVE: return "ENOACTIVE";
    #endif
    #ifdef ENOANO
        case ENOANO: return "ENOANO";
    #endif
    #ifdef ENOATTR
        case ENOATTR: return "ENOATTR";
    #endif
    #ifdef ENOBUFS
        case ENOBUFS: return "ENOBUFS";
    #endif
    #ifdef ENOCONN
        case ENOCONN: return "ENOCONN";
    #endif
    #ifdef ENOCONNECT
        case ENOCONNECT: return "ENOCONNECT";
    #endif
    #ifdef ENOCSI
        case ENOCSI: return "ENOCSI";
    #endif
    #ifdef ENODATA
        case ENODATA: return "ENODATA";
    #endif
    #ifdef ENODEV
        case ENODEV: return "ENODEV";
    #endif
    #ifdef ENODUST
        case ENODUST: return "ENODUST";
    #endif
    #ifdef ENOENT
        case ENOENT: return "ENOENT";
    #endif
    #ifdef ENOEXEC
        case ENOEXEC: return "ENOEXEC";
    #endif
    #ifdef ENOGRACE
        case ENOGRACE: return "ENOGRACE";
    #endif
    #ifdef ENOIOCTL
        case ENOIOCTL: return "ENOIOCTL";
    #endif
    #ifdef ENOIOCTLCMD
        case ENOIOCTLCMD: return "ENOIOCTLCMD";
    #endif
    #ifdef ENOKEY
        case ENOKEY: return "ENOKEY";
    #endif
    #ifdef ENOLCK
        case ENOLCK: return "ENOLCK";
    #endif
    #ifdef ENOLIC
        case ENOLIC: return "ENOLIC";
    #endif
    #ifdef ENOLINK
        case ENOLINK: return "ENOLINK";
    #endif
    #ifdef ENOLOAD
        case ENOLOAD: return "ENOLOAD";
    #endif
    #ifdef ENOMATCH
        case ENOMATCH: return "ENOMATCH";
    #endif
    #ifdef ENOMEDIUM
        case ENOMEDIUM: return "ENOMEDIUM";
    #endif
    #ifdef ENOMEM
        case ENOMEM: return "ENOMEM";
    #endif
    #ifdef ENOMOVE
        case ENOMOVE: return "ENOMOVE";
    #endif
    #ifdef ENOMSG
        case ENOMSG: return "ENOMSG";
    #endif
    #ifdef ENONDP
        case ENONDP: return "ENONDP";
    #endif
    #ifdef ENONET
        case ENONET: return "ENONET";
    #endif
    #ifdef ENOPARAM
        case ENOPARAM: return "ENOPARAM";
    #endif
    #ifdef ENOPARTNERINFO
        case ENOPARTNERINFO: return "ENOPARTNERINFO";
    #endif
    #ifdef ENOPKG
        case ENOPKG: return "ENOPKG";
    #endif
    #ifdef ENOPOLICY
        case ENOPOLICY: return "ENOPOLICY";
    #endif
    #ifdef ENOPROTOOPT
        case ENOPROTOOPT: return "ENOPROTOOPT";
    #endif
    #ifdef ENOREG
        case ENOREG: return "ENOREG";
    #endif
    #ifdef ENOREMOTE
        case ENOREMOTE: return "ENOREMOTE";
    #endif
    #ifdef ENORESOURCES
        case ENORESOURCES: return "ENORESOURCES";
    #endif
    #ifdef ENOREUSE
        case ENOREUSE: return "ENOREUSE";
    #endif
    #ifdef ENOSHARE
        case ENOSHARE: return "ENOSHARE";
    #endif
    #ifdef ENOSPC
        case ENOSPC: return "ENOSPC";
    #endif
    #ifdef ENOSR
        case ENOSR: return "ENOSR";
    #endif
    #ifdef ENOSTR
        case ENOSTR: return "ENOSTR";
    #endif
    #ifdef ENOSYM
        case ENOSYM: return "ENOSYM";
    #endif
    #ifdef ENOSYS
        case ENOSYS: return "ENOSYS";
    #endif
    #ifdef ENOSYSTEM
        case ENOSYSTEM: return "ENOSYSTEM";
    #endif
    #ifdef ENOTACTIVE
        case ENOTACTIVE: return "ENOTACTIVE";
    #endif
    #ifdef ENOTAUTH
        case ENOTAUTH: return "ENOTAUTH";
    #endif
    #ifdef ENOTBLK
        case ENOTBLK: return "ENOTBLK";
    #endif
    #ifdef ENOTCAPABLE
        case ENOTCAPABLE: return "ENOTCAPABLE";
    #endif
    #ifdef ENOTCONN
        case ENOTCONN: return "ENOTCONN";
    #endif
    #ifdef ENOTDIR
        case ENOTDIR: return "ENOTDIR";
    #endif
    #ifdef ENOTEMPT
        case ENOTEMPT: return "ENOTEMPT";
    #endif
    #ifdef ENOTEMPTY
        case ENOTEMPTY: return "ENOTEMPTY";
    #endif
    #ifdef ENOTNAM
        case ENOTNAM: return "ENOTNAM";
    #endif
    #ifdef ENOTREADY
        case ENOTREADY: return "ENOTREADY";
    #endif
    #ifdef ENOTRECOVERABLE
        case ENOTRECOVERABLE: return "ENOTRECOVERABLE";
    #endif
    #ifdef ENOTRUST
        case ENOTRUST: return "ENOTRUST";
    #endif
    #ifdef ENOTSOCK
        case ENOTSOCK: return "ENOTSOCK";
    #endif
    #ifdef ENOTSUP
        case ENOTSUP: return "ENOTSUP";
    #endif
    #ifdef ENOTSUPP
        case ENOTSUPP: return "ENOTSUPP";
    #endif
    #ifdef ENOTSYNC
        case ENOTSYNC: return "ENOTSYNC";
    #endif
    #ifdef ENOTTY
        case ENOTTY: return "ENOTTY";
    #endif
    #ifdef ENOTUNIQ
        case ENOTUNIQ: return "ENOTUNIQ";
    #endif
    #ifdef ENOUNLD
        case ENOUNLD: return "ENOUNLD";
    #endif
    #ifdef ENOUNREG
        case ENOUNREG: return "ENOUNREG";
    #endif
    #ifdef ENOURG
        case ENOURG: return "ENOURG";
    #endif
    #ifdef ENXIO
        case ENXIO: return "ENXIO";
    #endif
    #ifdef EOFFLOADboxDOWN
        case EOFFLOADboxDOWN: return "EOFFLOADboxDOWN";
    #endif
    #ifdef EOFFLOADboxERROR
        case EOFFLOADboxERROR: return "EOFFLOADboxERROR";
    #endif
    #ifdef EOFFLOADboxRESTART
        case EOFFLOADboxRESTART: return "EOFFLOADboxRESTART";
    #endif
    #ifdef EOPCOMPLETE
        case EOPCOMPLETE: return "EOPCOMPLETE";
    #endif
    #ifdef EOPENSTALE
        case EOPENSTALE: return "EOPENSTALE";
    #endif
    #ifdef EOPNOTSUPP
        #if !defined(ENOTSUP) || EOPNOTSUPP != ENOTSUP
        case EOPNOTSUPP: return "EOPNOTSUPP";
        #endif
    #endif
    #ifdef EOUTOFSTATE
        case EOUTOFSTATE: return "EOUTOFSTATE";
    #endif
    #ifdef EOVERFLOW
        case EOVERFLOW: return "EOVERFLOW";
    #endif
    #ifdef EOWNERDEAD
        case EOWNERDEAD: return "EOWNERDEAD";
    #endif
    #ifdef EPACKSIZE
        case EPACKSIZE: return "EPACKSIZE";
    #endif
    #ifdef EPASSTHROUGH
        case EPASSTHROUGH: return "EPASSTHROUGH";
    #endif
    #ifdef EPATHREMOTE
        case EPATHREMOTE: return "EPATHREMOTE";
    #endif
    #ifdef EPERM
        case EPERM: return "EPERM";
    #endif
    #ifdef EPFNOSUPPORT
        case EPFNOSUPPORT: return "EPFNOSUPPORT";
    #endif
    #ifdef EPIPE
        case EPIPE: return "EPIPE";
    #endif
    #ifdef EPOWERF
        case EPOWERF: return "EPOWERF";
    #endif
    #ifdef EPROBE_DEFER
        case EPROBE_DEFER: return "EPROBE_DEFER";
    #endif
    #ifdef EPROCLIM
        case EPROCLIM: return "EPROCLIM";
    #endif
    #ifdef EPROCUNAVAIL
        case EPROCUNAVAIL: return "EPROCUNAVAIL";
    #endif
    #ifdef EPROGMISMATCH
        case EPROGMISMATCH: return "EPROGMISMATCH";
    #endif
    #ifdef EPROGUNAVAIL
        case EPROGUNAVAIL: return "EPROGUNAVAIL";
    #endif
    #ifdef EPROTO
        case EPROTO: return "EPROTO";
    #endif
    #ifdef EPROTONOSUPPORT
        case EPROTONOSUPPORT: return "EPROTONOSUPPORT";
    #endif
    #ifdef EPROTOTYPE
        case EPROTOTYPE: return "EPROTOTYPE";
    #endif
    #ifdef EPWROFF
        case EPWROFF: return "EPWROFF";
    #endif
    #ifdef EQFULL
        case EQFULL: return "EQFULL";
    #endif
    #ifdef EQSUSPENDED
        case EQSUSPENDED: return "EQSUSPENDED";
    #endif
    #ifdef ERANGE
        case ERANGE: return "ERANGE";
    #endif
    #ifdef ERECALLCONFLICT
        case ERECALLCONFLICT: return "ERECALLCONFLICT";
    #endif
    #ifdef ERECURSE
        case ERECURSE: return "ERECURSE";
    #endif
    #ifdef ERECYCLE
        case ERECYCLE: return "ERECYCLE";
    #endif
    #ifdef EREDRIVEOPEN
        case EREDRIVEOPEN: return "EREDRIVEOPEN";
    #endif
    #ifdef EREFUSED
        #if !defined(ECONNREFUSED) || EREFUSED != ECONNREFUSED
        case EREFUSED: return "EREFUSED";
        #endif
    #endif
    #ifdef ERELOC
        case ERELOC: return "ERELOC";
    #endif
    #ifdef ERELOCATED
        case ERELOCATED: return "ERELOCATED";
    #endif
    #ifdef ERELOOKUP
        case ERELOOKUP: return "ERELOOKUP";
    #endif
    #ifdef EREMCHG
        case EREMCHG: return "EREMCHG";
    #endif
    #ifdef EREMDEV
        case EREMDEV: return "EREMDEV";
    #endif
    #ifdef EREMOTE
        case EREMOTE: return "EREMOTE";
    #endif
    #ifdef EREMOTEIO
        case EREMOTEIO: return "EREMOTEIO";
    #endif
    #ifdef EREMOTERELEASE
        case EREMOTERELEASE: return "EREMOTERELEASE";
    #endif
    #ifdef ERESTART
        case ERESTART: return "ERESTART";
    #endif
    #ifdef ERESTARTNOHAND
        case ERESTARTNOHAND: return "ERESTARTNOHAND";
    #endif
    #ifdef ERESTARTNOINTR
        case ERESTARTNOINTR: return "ERESTARTNOINTR";
    #endif
    #ifdef ERESTARTSYS
        case ERESTARTSYS: return "ERESTARTSYS";
    #endif
    #ifdef ERESTART_RESTARTBLOCK
        case ERESTART_RESTARTBLOCK: return "ERESTART_RESTARTBLOCK";
    #endif
    #ifdef ERFKILL
        case ERFKILL: return "ERFKILL";
    #endif
    #ifdef EROFS
        case EROFS: return "EROFS";
    #endif
    #ifdef ERPCMISMATCH
        case ERPCMISMATCH: return "ERPCMISMATCH";
    #endif
    #ifdef ERREMOTE
        case ERREMOTE: return "ERREMOTE";
    #endif
    #ifdef ESAD
        case ESAD: return "ESAD";
    #endif
    #ifdef ESECTYPEINVAL
        case ESECTYPEINVAL: return "ESECTYPEINVAL";
    #endif
    #ifdef ESERVERFAULT
        case ESERVERFAULT: return "ESERVERFAULT";
    #endif
    #ifdef ESHLIBVERS
        case ESHLIBVERS: return "ESHLIBVERS";
    #endif
    #ifdef ESHUTDOWN
        case ESHUTDOWN: return "ESHUTDOWN";
    #endif
    #ifdef ESIGPARM
        case ESIGPARM: return "ESIGPARM";
    #endif
    #ifdef ESOCKETNOTALLOCATED
        case ESOCKETNOTALLOCATED: return "ESOCKETNOTALLOCATED";
    #endif
    #ifdef ESOCKETNOTDEFINED
        case ESOCKETNOTDEFINED: return "ESOCKETNOTDEFINED";
    #endif
    #ifdef ESOCKTNOSUPPORT
        case ESOCKTNOSUPPORT: return "ESOCKTNOSUPPORT";
    #endif
    #ifdef ESOFT
        case ESOFT: return "ESOFT";
    #endif
    #ifdef ESPIPE
        case ESPIPE: return "ESPIPE";
    #endif
    #ifdef ESRCH
        case ESRCH: return "ESRCH";
    #endif
    #ifdef ESRMNT
        case ESRMNT: return "ESRMNT";
    #endif
    #ifdef ESRVRFAULT
        case ESRVRFAULT: return "ESRVRFAULT";
    #endif
    #ifdef ESTALE
        case ESTALE: return "ESTALE";
    #endif
    #ifdef ESTRPIPE
        case ESTRPIPE: return "ESTRPIPE";
    #endif
    #ifdef ESUBTASKALREADYACTIVE
        case ESUBTASKALREADYACTIVE: return "ESUBTASKALREADYACTIVE";
    #endif
    #ifdef ESUBTASKINVALID
        case ESUBTASKINVALID: return "ESUBTASKINVALID";
    #endif
    #ifdef ESUBTASKNOTACTIVE
        case ESUBTASKNOTACTIVE: return "ESUBTASKNOTACTIVE";
    #endif
    #ifdef ESYSERROR
        case ESYSERROR: return "ESYSERROR";
    #endif
    #ifdef ETERM
        case ETERM: return "ETERM";
    #endif
    #ifdef ETIME
        case ETIME: return "ETIME";
    #endif
    #ifdef ETIMEDOUT
        case ETIMEDOUT: return "ETIMEDOUT";
    #endif
    #ifdef ETOOMANYREFS
        case ETOOMANYREFS: return "ETOOMANYREFS";
    #endif
    #ifdef ETOOSMALL
        case ETOOSMALL: return "ETOOSMALL";
    #endif
    #ifdef ETRAPDENIED
        case ETRAPDENIED: return "ETRAPDENIED";
    #endif
    #ifdef ETXTBSY
        case ETXTBSY: return "ETXTBSY";
    #endif
    #ifdef ETcpBadObj
        case ETcpBadObj: return "ETcpBadObj";
    #endif
    #ifdef ETcpClosed
        case ETcpClosed: return "ETcpClosed";
    #endif
    #ifdef ETcpErr
        case ETcpErr: return "ETcpErr";
    #endif
    #ifdef ETcpLinked
        case ETcpLinked: return "ETcpLinked";
    #endif
    #ifdef ETcpOutOfState
        case ETcpOutOfState: return "ETcpOutOfState";
    #endif
    #ifdef ETcpUnattach
        case ETcpUnattach: return "ETcpUnattach";
    #endif
    #ifdef EUCLEAN
        case EUCLEAN: return "EUCLEAN";
    #endif
    #ifdef EUNATCH
        case EUNATCH: return "EUNATCH";
    #endif
    #ifdef EUNKNOWN
        case EUNKNOWN: return "EUNKNOWN";
    #endif
    #ifdef EURG
        case EURG: return "EURG";
    #endif
    #ifdef EUSERS
        case EUSERS: return "EUSERS";
    #endif
    #ifdef EVERSION
        case EVERSION: return "EVERSION";
    #endif
    #ifdef EWOULDBLOCK
        #if !defined(EAGAIN) || EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK: return "EWOULDBLOCK";
        #endif
    #endif
    #ifdef EWRONGFS
        case EWRONGFS: return "EWRONGFS";
    #endif
    #ifdef EWRPROTECT
        case EWRPROTECT: return "EWRPROTECT";
    #endif
    #ifdef EXDEV
        case EXDEV: return "EXDEV";
    #endif
    #ifdef EXFULL
        case EXFULL: return "EXFULL";
    #endif
    }
    return 0;
}

#endif /* ERRNONAME_C */
