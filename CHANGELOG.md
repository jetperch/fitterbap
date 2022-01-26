
# CHANGELOG

This file contains the list of changes made to the Fitterbap library.


## 0.5.0

2022 Jan 26 [in progress]

* Restructured platform and configuration.
  * Allow for static inline functions in platform.
  * Clarified platform porting requirements.
  * Removed "fitterbap/lib.h" since no longer needed.
* Updated comm/framer.h analysis based upon
  [Martin Cowen's](http://blog.martincowen.me.uk/using-and-misusing-crcs.html)
  feedback.  Thank you!
* Added uart_thread_tester.
* Improved log handler.
* Improved host UART: Added DTR and TX callback.
* Added TX event to UART thread tester for CPU efficiency.
* Added compile-time option to display frames in framer_test.
* Fixed gcc 11.2 errors and warnings.
* Fixed comm data link.
  * Improved and simplified reset handling.  Added sequence diagrams.
  * Removed comm send timeout.  Implement in app and higher levels as needed.
  * Fixed endless immediate scheduling when lower level transmitter is full.
  * Improved comm framer performance - no copy when entire frame in buffer.
  * Added process_request callback for more reliable OS integration.
  * Improved multi-threaded performance and fixed potential deadlock.
    Clarified that fbp_dl_send is the only thread-safe function.
  * Added fbp_dl_process and removed fbp_evm integration to give better
    decoupling and performance.
  * Added explicit buffer size to framer construct_data.
  * Fixed stream_tester.
  * Removed uart_thread and improved uart API.
    Changed from uart_thread_tester to uart_tester.
  * Added test/comm/comm.c
  * Modified test/comm/host.c to work as either server or client.
* Fixed python to better support PySide6.
* Added fbp_rbu64_is_empty
* Refactored windows error handling support.


## 0.4.1

2021 Jun 22

*   Removed unused variables.
*   Updated comm_ui log view widget to:
    *    Display most recent entry.
    *    Automatically size columns and automatically expand message.
*   Added null pointer check to fbp_evm_schedule().
*   Added pubsub "format" metadata to support major.minor.patch u32 version.
*   Added fbp_time_to_str().
*   Attempted Github Actions integration with start of linux support.


## 0.4.0

2021 Jun 14

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
*   Added log message port to comm stack and adjusted default log verbosity.
*   Added timesync with trivial, fixed-point, SNTP-like implementation.
*   Integrated logp changes into host, python, & UI.
    *   Connected logp messages to python host
    *   Add logp display widget to comm_ui.
    *   Migrated from PySide2 to PySide6.


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
