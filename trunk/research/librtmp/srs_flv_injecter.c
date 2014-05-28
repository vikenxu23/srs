/*
The MIT License (MIT)

Copyright (c) 2013-2014 winlin

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/**
gcc srs_flv_injecter.c ../../objs/lib/srs_librtmp.a -g -O0 -lstdc++ -o srs_flv_injecter
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../../objs/include/srs_librtmp.h"
#include "srs_research_public.h"
#include "srs_flv_codec.h"

int process(const char* in_flv_file, const char* out_flv_file, srs_flv_t* pic, srs_flv_t* poc);
int inject_flv(srs_flv_t ic, srs_flv_t oc);
int main(int argc, char** argv)
{
    int ret = 0;
    
    // user options.
    char* in_flv_file;
    char* out_flv_file;
    // flv handler
    srs_flv_t ic = NULL;
    srs_flv_t oc = NULL;
    
    // temp variables.
    int tmp_file_size = 0;
    char* tmp_file;
    
    if (argc <= 2) {
        printf("inject flv file keyframes to metadata\n"
            "Usage: %s in_flv_file out_flv_file\n"
            "   in_flv_file         input flv file to inject.\n"
            "   out_flv_file        the inject output file, can be in_flv_file.\n"
            "For example:\n"
            "   %s ../../doc/source.200kbps.768x320.flv injected.flv\n",
            argv[0]);
        ret = 1;
        exit(ret);
        return ret;
    }
    
    in_flv_file = argv[1];
    out_flv_file = argv[2];
    
    tmp_file_size = strlen(out_flv_file) + strlen(".tmp") + 1;
    tmp_file = (char*)malloc(tmp_file_size);
    snprintf(tmp_file, tmp_file_size, "%s.tmp", out_flv_file);
    
    trace("inject flv file keyframes to metadata.");
    trace("srs(simple-rtmp-server) client librtmp library.");
    trace("version: %d.%d.%d", srs_version_major(), srs_version_minor(), srs_version_revision());
    trace("input:  %s", in_flv_file);
    trace("output:  %s", out_flv_file);
    trace("tmp_file:  %s", tmp_file);

    ret = process(in_flv_file, tmp_file, &ic, &oc);
    
    srs_flv_close(ic);
    srs_flv_close(oc);
    
    if (ret != 0) {
        unlink(tmp_file);
        trace("error, remove tmp file.");
    } else {
        rename(tmp_file, out_flv_file);
        trace("completed, rename to %s", out_flv_file);
    }
    
    free(tmp_file);
    
    return ret;
}

int process(const char* in_flv_file, const char* out_flv_file, srs_flv_t* pic, srs_flv_t* poc)
{
    int ret = 0;
    
    if ((*pic = srs_flv_open_read(in_flv_file)) == NULL) {
        ret = 2;
        trace("open input flv file failed. ret=%d", ret);
        return ret;
    }
    
    if ((*poc = srs_flv_open_write(out_flv_file)) == NULL) {
        ret = 2;
        trace("open output flv file failed. ret=%d", ret);
        return ret;
    }
    
    if ((ret = inject_flv(*pic, *poc)) != 0) {
        return ret;
    }
    
    return ret;
}

int parse_metadata(char* data, int size, srs_amf0_t* pname, srs_amf0_t* pdata)
{
    int ret = 0;
    
    int nparsed = 0;
    *pname = srs_amf0_parse(data, size, &nparsed);
    
    if (*pname == NULL || nparsed >= size) {
        trace("invalid amf0 name data.");
        return -1;
    }
    
    *pdata = srs_amf0_parse(data + nparsed, size - nparsed, &nparsed);
    if (*pdata == NULL || nparsed > size) {
        trace("invalid amf0 value data");
        return -1;
    }
    
    return ret;
}

int inject_flv(srs_flv_t ic, srs_flv_t oc)
{
    int ret = 0;
    
    // flv header
    char header[13];
    // packet data
    char type;
    u_int32_t timestamp = 0;
    char* data = NULL;
    int32_t size;
    
    // metadata
    srs_amf0_t amf0_name = NULL;
    srs_amf0_t amf0_data = NULL;
    
    // reset to generate metadata
    srs_flv_lseek(ic, 0);
    
    if ((ret = srs_flv_read_header(oc, header)) != 0) {
        return ret;
    }
    
    trace("start inject flv");
    for (;;) {
        // tag header
        if ((ret = srs_flv_read_tag_header(oc, &type, &size, &timestamp)) != 0) {
            if (srs_flv_is_eof(ret)) {
                trace("parse completed.");
                return 0;
            }
            trace("flv get packet failed. ret=%d", ret);
            return ret;
        }
        
        if (size <= 0) {
            trace("invalid size=%d", size);
            break;
        }
        
        // TODO: FIXME: mem leak when error.
        data = (char*)malloc(size);
        if ((ret = srs_flv_read_tag_data(oc, data, size)) != 0) {
            return ret;
        }
        
        // data tag
        if (type == SRS_RTMP_TYPE_VIDEO) {
        } else if (type == SRS_RTMP_TYPE_SCRIPT) {
            if ((ret = parse_metadata(data, size, &amf0_name, &amf0_data)) != 0) {
                return ret;
            }
        }
        
        free(data);
    }
    
    srs_amf0_free(amf0_name);
    srs_amf0_free(amf0_data);
    
    return ret;
}