#pragma once

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// signal_handler.h
// dbien: 13MAR2020
// Catch signals and then print a stack trace with symbols and line numbers.

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <execinfo.h>
#include <stdio.h>
#include <err.h>

template < const int t_kiInstance >
class DefaultSignalHandler
{
public:
    enum _ESigType : char
    {
        estSIGSEGV,
        estSIGFPE,
        estSIGINT,
        estSIGILL,
        estSIGTERM,
        estSIGABRT,
        estESigTypeCount
    };
    typedef _ESigType ESigType;
    static struct sigaction s_rgsaOldSignalAction[ estESigTypeCount ]; // Save the old signal action so we can forward the signal.

    static void GetSignalNames( int _nSignal, siginfo_t * _psiSigInfo, ESigType & _rest, const char *& _rpszSig, const char *& _rpszCode )
    {
        _rpszSig = 0;
        _rpszCode = 0;
        switch (_nSignal)
        {
        case SIGSEGV:
            _rest = estSIGSEGV;
            _rpszSig = "SIGSEGV";
            break;
        case SIGINT:
            _rest = estSIGINT;
            _rpszSig = "SIGINT";
            break;
        case SIGFPE:
            _rest = estSIGFPE;
            _rpszSig = "SIGFPE";
            switch (_psiSigInfo->si_code)
            {
            case FPE_INTDIV:
                _rpszCode = "INTDIV";
                break;
            case FPE_INTOVF:
                _rpszCode = "INTOVF";
                break;
            case FPE_FLTDIV:
                _rpszCode = "FLTDIV";
                break;
            case FPE_FLTOVF:
                _rpszCode = "FLTOVF";
                break;
            case FPE_FLTUND:
                _rpszCode = "FLTUND";
                break;
            case FPE_FLTRES:
                _rpszCode = "FLTRES";
                break;
            case FPE_FLTINV:
                _rpszCode = "FLTINV";
                break;
            case FPE_FLTSUB:
                _rpszCode = "FLTSUB";
                break;
            default:
                break;
            }
        case SIGILL:
            _rest = estSIGILL;
            _rpszSig = "SIGILL";
            switch (_psiSigInfo->si_code)
            {
            case ILL_ILLOPC:
                _rpszCode = "ILLOPC";
                break;
            case ILL_ILLOPN:
                _rpszCode = "ILLOPN";
                break;
            case ILL_ILLADR:
                _rpszCode = "ILLADR";
                break;
            case ILL_ILLTRP:
                _rpszCode = "ILLTRP";
                break;
            case ILL_PRVOPC:
                _rpszCode = "PRVOPC";
                break;
            case ILL_PRVREG:
                _rpszCode = "PRVREG";
                break;
            case ILL_COPROC:
                _rpszCode = "COPROC";
                break;
            case ILL_BADSTK:
                _rpszCode = "BADSTK";
                break;
            default:
                break;
            }
        case SIGTERM:
            _rest = estSIGTERM;
            _rpszSig = "SIGTERM";
            break;
        case SIGABRT:
            _rest = estSIGABRT;
            _rpszSig = "SIGABRT";
            break;
        default:
            _rest = estESigTypeCount;
            break;
        }
    }

    // This is the default signal handler for terminating signals.
    static void DefaultHandler( int _nSignal, siginfo_t * _psiSigInfo, void *context )
    {
        // Check 
        ESigType restSigType;
        const char * pszSig;
        const char * pszCode;
        GetSignalNames( _nSignal, _psiSigInfo, restSigType, pszSig, pszCode );
        if ( estESigTypeCount != restSigType )
        {
            // 
            PosixLogStackTrace( restSigType, pszSig, pszCode, _nSignal, siginfo, context );

        }
        else
            n_SysLog::Log( eslmtError, "DefaultSignalHandler::DefaultHandler(): Unrecognized signal received [%d].", _nSignal );
        

        // If we have an old handler then call it:
        struct sigaction & rsaOldActiom = s_rgsaOldSignalAction[ estESigTypeCount ]        
    }

    static stack_t s_ssOldSigAltStack; // Save this to check out in the debugger.
    static void SetupAlternateSignalStack()
    {
        // Only create the stack if we intend to use it.
        static std::unique_ptr<uint8_t[]> p( DBG_NEW uint8_t[ SIGSTKSZ ] );
        stack_t ss = {};
        ss.ss_sp = &p[0];
        ss.ss_size = SIGSTKSZ;
        ss.ss_flags = 0;
        PrepareErrNo();
        if ( sigaltstack( &ss, &s_ssOldSigAltStack ) != 0 )
            n_SysLog::Log( eslmtError, GetLastErrNo(), "DefaultSignalHandler::SetupAlternateSignalStack(): sigaltstack() failed." );
    }

    // Allow the caller to pass in a method call to determine how we will treat a given signal.
    // This will also allow them to disable the separate signal stack at runtime to allow for stack dumping, etc.
    static void SetupDefaultSignalHandler( bool _fUseAlternateSignalStack )
    {
        s_fUseAlternateSignalStack = _fUseAlternateSignalStack;
        if ( s_fUseAlternateSignalStack )
            SetupAlternateSignalStack();

        // 

        
    /* register our signal handlers */
    {
        struct sigaction sig_action = {};
        sig_action.sa_sigaction = posix_signal_handler;
        sigemptyset(&sig_action.sa_mask);

#ifdef __APPLE__
        /* for some reason we backtrace() doesn't work on osx
           when we use an alternate stack */
        sig_action.sa_flags = SA_SIGINFO;
#else
        sig_action.sa_flags = SA_SIGINFO | SA_ONSTACK;
#endif

        if (sigaction(SIGSEGV, &sig_action, NULL) != 0)
        {
            err(1, "sigaction");
        }
        if (sigaction(SIGFPE, &sig_action, NULL) != 0)
        {
            err(1, "sigaction");
        }
        if (sigaction(SIGINT, &sig_action, NULL) != 0)
        {
            err(1, "sigaction");
        }
        if (sigaction(SIGILL, &sig_action, NULL) != 0)
        {
            err(1, "sigaction");
        }
        if (sigaction(SIGTERM, &sig_action, NULL) != 0)
        {
            err(1, "sigaction");
        }
        if (sigaction(SIGABRT, &sig_action, NULL) != 0)
        {
            err(1, "sigaction");
        }
    }
}

#define MAX_STACK_FRAMES 64
static void *stack_traces[MAX_STACK_FRAMES];
void posix_print_stack_trace()
{
  int i, trace_size = 0;
  char **messages = (char **)NULL;
 
  trace_size = backtrace(stack_traces, MAX_STACK_FRAMES);
  messages = backtrace_symbols(stack_traces, trace_size);
 
  /* skip the first couple stack frames (as they are this function and
     our handler) and also skip the last frame as it's (always?) junk. */
  // for (i = 3; i < (trace_size - 1); ++i)
  // we'll use this for now so you can see what's going on
  for (i = 0; i < trace_size; ++i)
  {
    if (addr2line(icky_global_program_name, stack_traces[i]) != 0)
    {
      printf("  error determining line # for: %s\n", messages[i]);
    }
 
  }
  if (messages) { free(messages); } 
}

/* Resolve symbol name and source location given the path to the executable 
   and an address */
int addr2line(char const * const program_name, void const * const addr)
{
  char addr2line_cmd[512] = {0};
 
  /* have addr2line map the address to the relevant line in the code */
  #ifdef __APPLE__
    /* apple does things differently... */
    sprintf(addr2line_cmd,"atos -o %.256s %p", program_name, addr); 
  #else
    sprintf(addr2line_cmd,"addr2line -f -p -e %.256s %p", program_name, addr); 
  #endif
 
  /* This will print a nicely formatted string specifying the
     function and source line of the address */
  return system(addr2line_cmd);
}

} // namespace n_Signals
