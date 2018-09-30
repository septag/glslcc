## sjson: Fast and portable C single header json Encoder/Decoder

This is actually a fork of Joseph's awesome Json encoder/decoder code from his [repo](https://github.com/rustyrussell/ccan/tree/master/ccan/json).    
If you want to see the performance and analysis of the original encoder/decoder (which is _ccan/json_) visit [here](https://github.com/miloyip/nativejson-benchmark).  
The encoder/decoder code is almost the same. What I did was adding object pools and string pages (sjson_context)
that eliminates many micro memory allocations, which should improve encode/decode speed and data access performance. 
I also added malloc/free and libc API overrides, and made the library single header, so it makes it very easy to integrate it into other programs

### Features

- Single header C-file
- UTF8 support
- Fast with minimal allocations (Internal Object pool, String pool, ..)
- Overriable libc functions like malloc/free/memcpy/.. 
- Supports both Json encoding and decoding
- Encoder supports prettify 
- No dependencies
- Simple and easy to use C-API

### Usage

```c
#define SJSON_IMPLEMENTATION
#include "sjson.h"	
```

For more information, check out the header file itself.

[License (BSD 2-clause)](https://github.com/septag/sjson/blob/master/LICENSE)
--------------------------------------------------------------------------

<a href="http://opensource.org/licenses/BSD-2-Clause" target="_blank">
<img align="right" src="http://opensource.org/trademarks/opensource/OSI-Approved-License-100x137.png">
</a>

	Copyright 2018 Sepehr Taghdisian. All rights reserved.
	
	https://github.com/septag/sjson
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	   1. Redistributions of source code must retain the above copyright notice,
	      this list of conditions and the following disclaimer.
	
	   2. Redistributions in binary form must reproduce the above copyright notice,
	      this list of conditions and the following disclaimer in the documentation
	      and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS OR
	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
	EVENT SHALL COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
	INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
	OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

