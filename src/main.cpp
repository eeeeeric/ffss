// Create high quality, frame accurate* snapshots of video files
// *most of the time at least
// Copyright (C) 2013 Eric Zheng

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <boost/program_options.hpp>
#include <png++/png.hpp>

#include "ffms.h"

namespace po = boost::program_options;
using namespace std;

int FFMS_CC indexProgress(int64_t Current, int64_t Total, void *ICPrivate);

/* Much of the ffms2 init code (and related comments) is (are) taken directly from the documentation */

int main(int argc, char * argv[]) {
    /* Program Options */

    struct opts
    {
        string input;
        vector<size_t> frames;
        string output;
        string ffvideoindex;
        bool debug;
    } options;

    stringstream optionsIntro;
    optionsIntro << "ffss is a tool for taking high quality, frame accurate snapshots from videos using ffmpegsource (ffmpeg)\n";
    optionsIntro << "usage: ffss [options] INPUT";
    
    po::options_description desc("");
    po::variables_map vm;

    try
    {        
        desc.add_options()
            ("input,i", po::value<string>(&options.input)->required(), "input file")
            ("frame,f", po::value< vector<size_t> >(&options.frames)->required(), "frame to retrieve, may be specified multiple times")
            ("output,o", po::value<string>(&options.output)->default_value("snapshot"), "output base name, final in format output_${FRAMENUM}${FRAMETYPE}.png")
            ("index", po::value<string>(&options.ffvideoindex), "input/output ffvideoindex")
            ("debug,d", "enable debugging output")
            ("help,h", "print this help message and exit")
        ;

        po::positional_options_description p;
        p.add("input", -1);

        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
        
        if (argc == 1 || vm.count("help"))
        {
            /* need double endl to actually get one */
            cout << optionsIntro.str() << endl << endl;;
            cout << desc << endl;
            return 1;
        }

        /* notify after help check or else required options will break it */
        po::notify(vm);
        options.debug = vm.count("debug");

    }
    catch(exception & e) 
    {
        cerr << "error: " << e.what() << "\n";
        return 1;
    }
    catch(...) 
    {
        cerr << "Exception of unknown type!\n";
    }

    /* Use FFMS to access the video file */

    FFMS_Init(0, 0);

    /* Index the source file */
    char errmsg[1024];
    FFMS_ErrorInfo errinfo;
    errinfo.Buffer      = errmsg;
    errinfo.BufferSize  = sizeof(errmsg);
    errinfo.ErrorType   = FFMS_ERROR_SUCCESS;
    errinfo.SubType     = FFMS_ERROR_SUCCESS;
    const char *sourcefile = const_cast<char*>(options.input.c_str());

    FFMS_Index *index;
    bool makeIndex = true;
    bool readIndex = false;
    if (vm.count("index"))
    {
        if (options.debug)
            cout << "Attempting to read: " << options.ffvideoindex << endl;

        index = FFMS_ReadIndex(const_cast<char*>(options.ffvideoindex.c_str()), &errinfo);
        if (index == NULL)
        {
            cerr << errinfo.Buffer << endl;
        }
        else if (FFMS_IndexBelongsToFile(index, sourcefile, &errinfo) != 0)
        {
            cerr << errinfo.Buffer << endl;
        }
        else
        {
            makeIndex = false;
            readIndex = true;
        }
    }
    if (makeIndex)
    {
        if (options.debug)
            cout << "Creating new index.\n";

        int curprogress = 0;
        index = FFMS_MakeIndex(sourcefile, 0, 0, NULL, NULL, FFMS_IEH_ABORT, &indexProgress, &curprogress, &errinfo);
        if (curprogress < 100)
        {
            cout << "Indexing: 100%\n";
        }

        if (index == NULL)
        {
            cerr << errinfo.Buffer << endl << "Exiting." << endl;
            return 1;
        }
    }

    /* Retrieve the track number of the first video track */
    int trackno = FFMS_GetFirstTrackOfType(index, FFMS_TYPE_VIDEO, &errinfo);
    if (trackno < 0) {
        /* no video tracks found in the file, this is bad and you should handle it */
        /* (print the errmsg somewhere) */
        cerr << errinfo.Buffer << endl;
        cerr << "Exiting." << endl;
        return 1;
    }

    /* We now have enough information to create the video source object */
    FFMS_VideoSource *videosource = FFMS_CreateVideoSource(sourcefile, trackno, index, 1, FFMS_SEEK_NORMAL, &errinfo);
    if (videosource == NULL) {
        /* handle error (you should know what to do by now) */
        cerr << errinfo.Buffer << endl;
        cerr << "Exiting." << endl;
        return 1;
    }

    /* Save index if desired, destroy */
    if (vm.count("index") && !readIndex)
    {
        if (options.debug)
            cout << "Writing index to: " << options.ffvideoindex << endl;
        
        if (FFMS_WriteIndex(const_cast<char*>(options.ffvideoindex.c_str()), index, &errinfo) != 0)
        {
            cerr << errinfo.Buffer << endl;
        }
    }

    if (options.debug)
        cout << "Destroying index.\n";
    FFMS_DestroyIndex(index);
  
    /* Get the first frame for examination so we know what we're getting. This is required
    because resolution and colorspace is a per frame property and NOT global for the video. */
    const FFMS_Frame *propframe = FFMS_GetFrame(videosource, 0, &errinfo);

    if (options.debug)
    {
        cout << "Width: " << propframe->EncodedWidth << endl;
        cout << "Height: " << propframe->EncodedHeight << endl;
        cout << "PIX_FMT (frame colorspace): " << propframe->EncodedPixelFormat << endl;
    }

    /* RGB24 output, we're getting YUV->RGB conversion done for us!
       Unlike the binary version of ffmpeg, BT.709 will be used for HD */
    int pixfmt[2];
    pixfmt[0] = FFMS_GetPixFmt("rgb24");
    pixfmt[1] = -1;
    
    if (FFMS_SetOutputFormatV2(videosource, pixfmt, propframe->EncodedWidth, propframe->EncodedHeight, FFMS_RESIZER_BICUBIC, &errinfo)) {
        cerr << errinfo.Buffer << endl;
        cerr << "Exiting." << endl;
        return 1;
    }

    /* now we're ready to actually retrieve the video frames and save to PNG */
    for (size_t i = 0; i < options.frames.size(); i++)
    {
        int framenumber = options.frames[i]; /* valid until next call to FFMS_GetFrame* on the same video object */
        const FFMS_Frame *curframe = FFMS_GetFrame(videosource, framenumber, &errinfo);
        if (curframe == NULL) {
            /* handle error */
            cerr << errinfo.Buffer << endl;
            cerr << "ERROR: Skipping frame " << framenumber << endl;
            continue;
        }
        else
        {
            cout << "Current Frame: " << options.frames[i] << "\t" << "Frame Type: " << curframe->PictType << endl;
        }

        switch ( curframe->ColorSpace )
        {
            case FFMS_CS_RGB:
                cout << "Using ColorSpace FFMS_CS_RGB" << endl;
                break;
            case FFMS_CS_BT709:
                cout << "Using ColorSpace FFMS_CS_BT709" << endl;
                break;
            case FFMS_CS_UNSPECIFIED:
                cout << "Using ColorSpace FFMS_CS_UNSPECIFIED" << endl;
                break;
            case FFMS_CS_FCC:
                cout << "Using ColorSpace FFMS_CS_FCC" << endl;
                break;
            case FFMS_CS_BT470BG:
                cout << "Using ColorSpace FFMS_CS_BT470BG" << endl;
                break;
            case FFMS_CS_SMPTE170M:
                cout << "Using ColorSpace FFMS_CS_SMPTE170M" << endl;
                break;
            case FFMS_CS_SMPTE240M:
                cout << "Using ColorSpace FFMS_CS_SMPTE240M" << endl;
                break;
        }

        if (options.debug)
        {
            cout << "convertedpixelformat = " << curframe->ConvertedPixelFormat << endl;
            cout << "ScaledWidth: " << curframe->ScaledWidth << endl;
            cout << "ScaledHeight: " << curframe->ScaledHeight << endl;            
        }

        /* Create PNG */
        png::image< png::basic_rgb_pixel<uint8_t> > snapshot(curframe->EncodedWidth, curframe->EncodedHeight);

        for (int x = 0; x < curframe->EncodedWidth; x++)
        {
            for (int y = 0; y < curframe->EncodedHeight; y++)
            {
                png::basic_rgb_pixel<uint8_t> curpix;
                int redIndex = x*3 + (y*curframe->EncodedWidth*3);
                int greenIndex = redIndex + 1;
                int blueIndex = redIndex + 2;
                curpix.red = curframe->Data[0][redIndex];
                curpix.green = curframe->Data[0][greenIndex];
                curpix.blue = curframe->Data[0][blueIndex];
                snapshot.set_pixel(x, y, curpix);
            }
        }

        stringstream oss;
        oss << options.output << "_" << options.frames[i] << curframe->PictType << ".png";
        snapshot.write(oss.str());
    }

    /* now it's time to clean up */
    FFMS_DestroyVideoSource(videosource);
    
    return 0;
}

int FFMS_CC indexProgress(int64_t Current, int64_t Total, void *ICPrivate)
{
    int progress = (Current*100/Total);
    if (progress > *(int*)ICPrivate)
    {
        cout << "Indexing: " << progress << "% \r" << flush;
    }
    return 0;
}