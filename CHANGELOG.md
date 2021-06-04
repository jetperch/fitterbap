
# CHANGELOG

This file contains the list of changes made to the Fitterbap library.


## 0.4.0

2021 Jun 4

*   Improved comm stack based upon EOC2021 attendee feedback - thank you!
    *   Added framer length crc-8 and mandatory EOF match.
    *   Reduced metadata to 16-bit and port-data to 8-bit.
    *   Increased frame_type Hamming distance between data & link frames. 
    *   Improved state machines & timeouts.
    *   Added data link window size negotiation.    
    *   Improved port0 and pubsubp negotiation.
    *   Added FBP_DL_EV_TRANSPORT_CONNECTED and FBP_DL_EV_APP_CONNECTED.
    *   Simplified data link to always buffer using full-sized TX frames.
        Removed tx_buffer_size from fbp_dl_config_s.
    *   Added 32-bit CRC configuration option.
    *   Combined comm stack processing into event_manager.
*   Added fbp_os_current_task_id() to assist reentrant code implementations.
*   Modified fbp_evm_interval_next return code.  
    Returns INT64_MAX rather than -1 on no scheduled events to simplify caller.
*   Added fbp_evm_on_schedule for improved event_manager thread integration. 
*   Fixed deadlock between GIL and native mutex.
*   Added log message port to comm stack.


## 0.3.2

2021 May 15

*   Added native files to python package.


## 0.3.1

2021 May 15

*   Fixed pyfitterbap package
    *   Fixed python dependencies.
    *   Fixed version_update.py to correctly update include/version.h.
    *   Improved README.
    *   Fixed python console script "fitterbap" installed with package.


## 0.3.0

2021 May 7 

*   Initial public release.
    Clean fork with major changes from [EMBC](https://github.com/mliberty1/embc).
