# CCSDS 122.0-B-2 Implementation

CCSDS122x0b2_impl is implementation of [CCSDS 122.0-B-2](https://ccsds.org/wp-content/uploads/gravity_forms/5-448e85c647331d9cbaf66c096458bdd5/2025/01/122x0b2e1.pdf) ([local copy](docs/122x0b2.pdf) available in the repo).

This project aims to implement standard-compliant, portable, effective, high-performant and scalable solution that includes both encoders and decoders, that would utilize modern hardware capabilities, such as concurrency and vector-aware data processing.

All modules are implemented in C++ using Standard Library and have no third-party dependencies. Apart from CCSDS-related modules, the project contains tasking system, bit stream and IO utilities as part of a library, which are intended to be payload-agnostic and expected to be reusable for different purposes. 


### What is CCSDS 122.0-B-2?
CCSDS 122.0-B-2 is an image data compression standard governed by the Consultative Committee for Space Data Systems (CCSDS). The image processing approach proposed by the standard uses 9/7 Discrete Wavelet Transform and custom bitplane encoding technique, which includes applications of differential and Huffman codes, offering both lossless and lossy compression of images with effective pixel depth up to 28 bits (somewhat similar to JPEG2000, but JPEG2000 uses simpler 5/3 DWT transform and different encoding techniques).

### What's inside this repo?
`build` directory contains scripts necessary to build binaries (currently *msbuild* only). `doc` directory contains accompanying materials that are not part of the application or library itself.

`src` directory contains subdirectories for substituent modules, which in turn contain relevant source files. `dwt` and `bpe` (for Discrete Wavelet Transform and Bit Plane Encoding, respectively) module directories contain source files that implement image processing as described in the standard. `common` subdirectory contains utilities and type definitions used throughout all other modules. `io` contains logic and definitions for structures used for input/output and multithreaded data processing management. `core` subdirectory contains high-level logic that combines aforementioned modules into data processing back-end, represented as a library. `test` contains Google Tests for different modules. `cli` conatins implementation of command line interface that represents front-end for core library.

### You mentioned *performant* and *portable* in the same sentence amid the goals of this project. Isn't it contradictory?
Yep. For sake of gaining portability-related goals, this project does not use platform-specific or OS-specific API, neither it depends on third-party libraries (except gtest). Instead of using specific intrinsics or optimized low-level librarian solutions, this project relies on optimization capabilities of modern compilers. 

### So why not to use low-level OS API, isn't it much more performant compared to heavy standard library wrappers/alternatives?
It is assumed that need for system-specific interaction (exept for multithreaded synchronization and memory management) is limited to loading image data or compressed segment data from some kind of system media/storage in the beginning of image processing, and storing result data back in the end of processing session. All the data processing in between does not require any system-specific support and can be completely described within programming language models. That processing part is assumed to be very expensive in terms of computational resources, and therefore it is the primary object for optimizations, considering data loading and storing part much less consuming overal and therefore advantages of using optimized system-specific approaches having not enough gain (especially considering corresponding disadvantages in portability).  
As for multithreading and memory management support, corresponding libraries in the C++ Standard Library are considered being fairly versatile and comprehensive, so refusal of these libraries would lead to loss of generality (in addition to corresponsing losses in portability).

### If portability matters so much, how even to decide which approach is more efficient on some (and all) targeted platforms?
Target platform hardware assumed to posess the following properties:
- [multithreaded](https://en.wikipedia.org/wiki/Simultaneous_multithreading)
- [hierarchical memory](https://en.wikipedia.org/wiki/Memory_hierarchy)
- [vectorization support](https://en.wikipedia.org/wiki/Vector_processor)
- [superscalar](https://en.wikipedia.org/wiki/Superscalar_processor)
- [out-of-order](https://en.wikipedia.org/wiki/Out-of-order_execution)

All these assumptions are true for (*almost?*) any reasonable implementation of a platform that cares about performance in any extent, e.g. common general purpose architectures: [IA32-64](https://en.wikipedia.org/wiki/Pentium_4), [ARM](https://en.wikipedia.org/wiki/Apple_A7), RISC-V.  
For a hardware architecture that satisfies assumptions above, the following techiniques should provide performance benefits:
- multithreading
- vectorization
- overallocation
- weakening data dependencies
- weakening control dependencies

This project is merely experimental in regard of aiming portability, standard conformance and performance at once, questioning "*How hard can it be?*" and "*How good can the result be?*". Until portability is verified and performance/efficiency is measured/analyzed for different covered platforms, no claims regarding applicability of such approach are made; proving it is possible and efficient or disproving that are both valid outcomes.

### Why not GPU?
Despite image processing being a common domain for GPU-targeted tasks, image compression algorithms are characterized by very strong data dependencies, preventing them from reasonable and efficient implementation on GPU. E.g. integer DWT result for every image coefficient (pixel) depends heavily on results of intermediate computations for image coefficients in some neighbourhood, and the algorithm itself is not much simple (although uses primitive operations like addition and shifts); BPE processing implies current encoder/decoder state that is cumulative over all the previously encoded/decoded data.  
However, floating point DWT application consists of vector/matrix additions and multiplications only, with no intermediate result the output would depend on. That task can be effectively implemented on GPU. 
Floating point DWT is not implemented yet by this project: floating point DWT implies lossy compression, and the project implemented lossless compression first, because it is easier to verify and debug. It's unlikely that GPU implementation would be primary implementation for fp DWT due to portability issues related to programming on GPGPU, but it surely can be complimentary implementation on some set of supported platforms, providing additional performance advantages; that design choise is yet to be made.

### This compression algorithm seems very much sequential. What does the project have with multithreading?
Input image is divided into regions, to which DWT is applied concurrently. The result collection of fragments of subbands is clued into subband set representing some larger region of the image right before that larger region (or the whole image) is about to be divided into segments; segmentation is considered serializing operation in this regard. Separate segments are then encoded concurrently.  
Similarly, when decompressing the image from compressed segment data, segments are decoded concurrently (if storage structure allows to restore the order of all the input segments); then set of decoded segments are transformed into set of subbands. After subbands set is built, reverse DWT is applied concurrently to different regions, and the result image is composed by merging regions at the end of processing.

That is, the compression procedure can be split into parts, some parts can be executed concurrently, and others require synchronization and sequential execution. The project includes tasking system implementation that can effectively schedule chain (or tree) of tasks, the order of execution for which is known at compile time. For a front-end that would support several simultaneous compression sessions per application instance (e.g. service), the existing tasking implementation would ensure that sequential parts of individual sessions/channels are executed concurrently.

### But isn't premature optimization the root of [evil](https://softwareengineering.stackexchange.com/a/80092)?
Yes, 
> ... say about 97% of the time. (...) Yet we should not pass up our opportunities in that critical 3%.

In case of this particular project, many performance-driven decisions affected architecture of the implementation, to ensure that presumably critical parts can be further profiled and optimized without need to change the existing architecture. All in all, in a domain of image processing, where user expirience is significantly defined by product throughput specifications (and influence of performance-limiting factors, except for the product organization itself, is little), why even implement anything not bothering about performance?  
In addition, this project served to educational purposes for the author, and some details appeared as a result of personal interest of the author to some matter.

### But haven't you overcomplicated it?
[Yes](https://github.com/maxim-komlik/ccsds122x0b2_impl/blob/39ea93025bb9ad51cb126ed6d04d743550805dab/src/io/tasking.hpp#L413), [maybe](https://github.com/maxim-komlik/ccsds122x0b2_impl/blob/39ea93025bb9ad51cb126ed6d04d743550805dab/src/common/utils.hpp#L291), a [little](https://github.com/maxim-komlik/ccsds122x0b2_impl/blob/39ea93025bb9ad51cb126ed6d04d743550805dab/src/cli/utility.hpp#L280) [bit](https://github.com/maxim-komlik/ccsds122x0b2_impl/blob/39ea93025bb9ad51cb126ed6d04d743550805dab/src/dwt/dwt.cpp#L216).  
But this project also served to educational purposes for the author, and some details appeared as a result of personal interest of the author to some matter.

### I gave it a try, but it doesn't work. Why?
Because this project is still much work in progress. Here is a very incomplete list of known issues:
- error handling is mostly not implemented, application would either crash, either proceed with invalid data
- currently there's no way to supply user image data as input, only noise generation (kinda debugging option) as input is available (but support for BMP image format is planned as input and output)
- floating point DWT is not implemented
- controlled segment output data truncation is not implemented (i.e. options bitplane-stop, stage-stop and DC-stop)
- collection segment and stream parameters cannot be specified, single first element only
- multichannel images won't decompress (a bug, would probably cause crash)

### What's next?
The following features are planned for implementation:
- cmake build support
- error handling
- BMP image files as input/output
- floating point DWT and controlled stream truncation settings
- variadic length command line parameters
- custom (aligned over-)allocators implementation

### How to contribute?
Please feel free to share any thoughts on this project. Any feedback is valuable!  
Code reviews are much appreciated: any scope, any format.  
If you're interested in proposing changes to the codebase, consider submitting pull-request. The author will be happy to merge proposed improvements.
