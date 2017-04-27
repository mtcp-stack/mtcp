/* Copyright (c) 2014, Linaro Limited

 * All rights reserved
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**

@page api_guide_lines  API Guide Lines

@tableofcontents

@section introduction Introduction
ODP APIs are implemented as callable C functions that often return a typed value.
This document describes the approach to handling return values and error indications expected of conforming ODP implementations.
As such it should be regarded as providing guidelines for how to create new ODP APIs.

@section functional Functional Definition
This section defines the use of data types, calling conventions, and return codes used by ODP APIs.
All ODP APIs MUST follow these conventions as part of their design.

@subsection naming Naming Conventions
All ODP APIs begin with the prefix odp_ and those that describe an action to be performed on an object follow the naming convention of object followed by action.
The advantage of this approach is that an alphabetical list of APIs for an object all sort together since they all have names of the form odp_object_action().

So for example the API call to allocate a buffer is named odp_buffer_alloc() rather than odp_alloc_buffer().

@subsection data_types Data Types and Use of typedef
ODP is designed to allow broad variability in how APIs are implemented on various platforms.
To support this, most APIs operate on abstract data types that are defined via typedef on a per-implementation basis.
These abstract types follow the naming convention of odp_object_t.

Typedefs that encapsulate C structs follow the convention:

@code
typedef struct odp_<descriptive_name>_s {
...
} odp_<descriptive_name>_t;
@endcode

The use of typedef allows implementations to choose underlying data representations that map efficiently to platform capabilities while providing accessor functions to provide structured access to implementation information in a portable manner
Similarly, the use of enum is RECOMMENDED to provide value abstraction for API parameters while enabling the implementation to choose code points that map well to platform native values.

Several native C types are used conventionally within ODP and SHOULD be employed in API design:

type | Correct use
 |---| :---------
void | SHOULD be used for APIs that do not return a value
void*| SHOULD be used for APIs that return a pointer intended to be used by the caller. For example, a routine that returns the address of an application context area SHOULD use a void * return type
odp_bool_t  | SHOULD be used for APIs that return a @ref boolean value.
int  | SHOULD be used for success and failure indications, with 0 indicating a success. Errno may be set

@subsection parameters Parameter Structure and Validation
ODP is a framework for use in the data plane.
Data plane applications typically have extreme performance requirements mandating very strict attention to path length considerations in the design of all ODP APIs, with the exception of those designed to be used infrequently such as only during initialization or termination processing.

Minimizing pathlength in API design involves several considerations:
 - The number of parameters passed to a call.
   In general, ODP APIs designed for frequent use SHOULD have few parameters.
   Limiting parameter count to one or two well-chosen parameters SHOULD be the goal for APIs designed for frequent use.
   If a call requires more complex parameter data then it is RECOMMENDED that instead of multiple parameters a single pointer to a struct that can be statically templated and modified by the caller be used.
 - The use of macros and inlining.
   ODP APIs MAY be implemented as preprocessor macros and/or inline functions.
   This is especially true for accessor functions that are designed to provide getters/setters for object meta data.
 - Limiting parameter validation and error-checking processing.
   While useful for development and debugging, providing “bullet-proof” APIs that perform extensive parameter validation and error checking is often inappropriate.
   While validations that can be performed statically at compile time or at little to no runtime cost SHOULD be considered, APIs MAY choose to leave behavior as undefined when presented with invalid parameters in the interest of runtime efficiency.

One of the reasons for using abstract types is to avoid having implementation knowledge “bleed through” the API, leading to possible parameter errors.
When one API returns an opaque token to an application it is reasonable to expect that the application can pass that token to subsequent APIs without needing expensive runtime validation.

ODP provides the helper APIs ODP_STATIC_ASSERT(cond,msg) and ODP_ASSERT(cond,msg) that SHOULD be used in implementations for performing appropriate validation.
The former is a compile-time assertion and hence adds no additional path length.
The latter is controlled by the ODP_NO_DEBUG compile-time switch and so is suitable for use in development/debug builds that can be compiled out for production use.
Other mechanisms available to the implementer are:
 - ODP_ABORT() is provided for situations where further execution of the code must not occur and a level of tracing information should be left in the log.
 - ODP_DEPRECATED() is used to signify that a call is planned for obsolescence.
 - ODP_LOG() is used to direct implementation messages to the application.


@subsection function_name Function Names
Functions must attempt to be so clear in their intent that referencing the documentation is not necessary, the guidelines below should be followed unless a strong case is made for an exception.

@subsection getters Getting information

@subsubsection is_has Is / Has
An api with "is" or "has" are both considered @ref boolean questions. They can only return true or false and it reflects the current state of something.

An example might be a packet interface, you might want to know if it is in promiscuous mode.
@code odp_bool_t state = odp_pktio_is_promiscuous(pktio handle) @endcode

In addition you might want to know if it has the ability to be in promiscuous mode.
@code odp_bool_t state = odp_pktio_has_promiscuous(pktio handle) @endcode

Another case might be if a packet has a vlan flag set
@code odp_bool_t state = odp_packet_has_vlan(packet handle) @endcode

@subsubsection get Get
Where possible returned information should be an enum if it reflects a finite list of information.
In general get apis drop the actual tag "get" in the function name.

@subsection converter Converter Functions
To maintain efficiency in fastpath code converter functions should expect correct inputs with undefined results otherwise.

@code
static inline odp_foo_t _odp_foo_from_bar(odp_bar_t bar)
{
       return (odp_foo_t)bar;
}
@endcode

@subsection function_calls Function Calls
ODP APIs typically have prototypes of the form:

@code
odp_return_type_t odp_api(p1_type p1, p2_type p2, …);
@endcode
Where:

type              | Description
 |---------       | :---------
odp_return_type_t | Is the return value produced by the API call. As noted above, the native types void, void *, and int are also used. Other APIs return abstract types defined via typedef
p1_type           | Is the data type of the first parameter
p2_type           | Is the data type of the second parameter, etc.

For ODP APIs that return void, results are undefined if the input parameters are invalid.
For those that return void *, the value ODP_NULL or ODP_INVALID MAY be used to indicate call failure.
For non-boolean APIs returning int, a return value of 0 indicates success while non-zero indicates failure see @ref success.

@subsection errno Use of errno
ODP APIs SHOULD make use of the thread-local variable errno, defined in the standard library include file errno.h, to indicate a reason for an API call failure when appropriate.
This convention allows callers to easily determine success/failure of a call with a single test and then decode the failure as desired from the extended reason provided by errno.
So, for example, an attempt to allocate a buffer from a buffer pool might return ODP_BUFFER_INVALID if the call was unsuccessful and errno could then be set to an appropriate reason (no storage available (ENOMEM, ENOBUFS), pool not recognized (EINVAL), etc.).

In general APIs are free to define their own errno usage conventions and values or reuse standard errno values when appropriate.
When “standard” codes exist, implementations SHOULD make use of them so that standard utility functions like perror() can decode them intelligently.
There are, however, a small set of standard codes that are commonly used.
One errno value that MUST be present for all APIs is ODP_FUNCTION_NOT_AVAILABLE.
This special reason code is used to indicate that the underlying implementation does not support the requested API, and SHOULD be equated to ENOSYS.
This may be because the requested API is specifically designated as OPTIONAL or that the caller is using a pre-release version of an API that does not have all functionality implemented yet.

Another standard errno is ODP_IMPLEMENTATION_LIMIT.
This code SHOULD be used if a API call is made that exceeds a permitted limit of the underlying implementation, and SHOULD be equated to ERANGE.
For example, many APIs MAY mandate certain minimum functionality but provide latitude on maximums.
An example of this might be the number of queues that an application can create.
An attempt to allocate more queues than the underlying implementation supports would result in this failure code being returned via errno.

@subsection boolean Boolean
For odp all booleans are integers. To aid application readability they are defined as the type odp_bool_t.
The values  !0 = true, 0 = false are used for this purpose.

@subsection success Success and Failure
Pass indications are integers (int) and SHOULD also be used for APIs that return a simple success/failure indication to the caller.
In this case the return value 0 indicates success while non-zero (typically -1) indicates failure and errno is set to a reason code that indicates the nature of the failure.

@subsection odp_internal Internal APIs
When an interface is defined in a header file and is intended to to be reused internally it will follow these rules:-
- Be prefixed with an underscore "_".
- All the required definitions for the API are to use an underscore, this includes MACROS, typedefs, enums and function names.

@subsection variables Declaring variables
- Variables shall be declared at the beginning of scope, for example :-
@code
    int start_of_global_scope;

    main () {
      int start_of_function_scope;
      ...
      if (foo == bar) {
        int start_of_block_scope;
        ...
      }
    }
@endcode

@section implementation Implementation Considerations
To support application portability and preserve implementation flexibility, ODP APIs MUST be designed with several guiding principles in mind.

@subsection application_view Application View vs. Implementation View
ODP APIs MUST present an application view of a problem in their externals.
That is, the API should allow the application to specify what it wants to do while the underlying implementation of that API controls how the requested function is realized.
As a result, ODP APIs SHOULD NOT be designed with a specific implementation in mind.
This is the reason, for example, that packet I/O in ODP follows a queued model.
It is an implementation responsibility to determine how packets are physically read and written, and whatever internal structures are needed to perform this most efficiently are an implementation rather than an application concern.
In some platforms this may involve the use of receive rings and buffer bursting, while in others this may be a simple memory-mapped register operation to interface with a hardware packet scheduler/distributor.
The ODP application does not care how packets arrive for processing only that a packet is available for it to work on.

Similarly, ODP applications reference packets data fields in terms of the information that is needed, rather than focusing on how that information is obtained.
The assumption is that the underlying implementation has pre-parsed the packet to extract the most relevant data as packet meta data that is immediately available to the application without requiring the application to do this work itself.
Over time, as network speeds increase, more and higher level networking functions are expected to migrate directly into hardware and ODP APIs MUST be mindful of this evolution in their design.

@subsection essential_functions Essential functions vs. Extensions
At the same time, APIs SHOULD reflect essential needs of data plane application programming and SHOULD NOT strive to offer comprehensive solutions to every possible contingency.
How to draw this line is a judgement call based on experience but API designers MUST take implementation practicalities into consideration when designing APIs to ensure that APIs and features can be implemented efficiently on a wide variety of underlying platforms.
This is one of the reasons why some features MAY be defined as OPTIONAL.
While allowed, the proliferation of OPTIONAL features SHOULD be avoided to enable broad application portability across many implementations.
At the same time, a “least common denominator” approach MUST NOT be taken as that defeats the purpose of providing higher-level abstractions in APIs.

@subsection odp_deprecated ODP DEPRECATED
A deprecated API will remain marked as such in the public API using #ODP_DEPRECATED for two release cycles for the #ODP_VERSION_API_MAJOR number.
For example an API marked as deprecated in 1.1.0 will still be present in 1.2.0 and removed in 1.3.0.
A deprecated API will contain the doxygen tag \@deprecated with a description of the reason for the change.

@section defaults Default behaviours
When an API has a default behaviour it must be possible for the application to explicitly call for that behaviour, this guards against the default changing and breaking the application.

*/
