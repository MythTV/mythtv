// This header is based on Accellent by John Dagliesh:
//   http://www.defyne.org/dvb/accellent.html

#ifndef ACCEL_PRIVATE_H_
#define ACCEL_PRIVATE_H_

extern "C" {
typedef int CGSConnectionID;
typedef int CGSSurfaceID;
struct CGSObj { };
struct CGSBoolean : public CGSObj { };
struct CGSString : public CGSObj { };


CGSConnectionID _CGSDefaultConnection();

CGSConnectionID CGSMainConnectionID();
OSErr CGSGetWindowBounds( CGSConnectionID conn, int windowID, CGRect * rect );

void CGSAddSurface( CGSConnectionID conn, int windowID, CGSSurfaceID * surfaceID );
void CGSOrderSurface( CGSConnectionID conn, int windowID, CGSSurfaceID surfaceID, int unk1, int unk2 );
void CGSSetSurfaceBounds( CGSConnectionID conn, int windowID, CGSSurfaceID surfaceID, CGRect rect );

CGSBoolean * CGSCreateBoolean( bool val );
CGSString * CGSUniqueCString( char * str );
void CGSReleaseObj( CGSObj * obj );

void CGSSetWindowProperty( CGSConnectionID conn, int windowID, CGSString * key, CGSObj * value );


struct DVDVideoContext
{
};

typedef UInt64 uint64;
typedef UInt32 uint32;
typedef UInt16 uint16;
typedef unsigned char uint8;

typedef SInt64 sint64;
typedef SInt32 sint32;
typedef SInt16 sint16;
typedef signed char sint8;


#pragma pack(1)
struct CompletionInfo;

// Note: DVDPlayback keeps old mvs around unless it sees an explicit mv zero
// ... not even an intra mb will reset them to zero
struct MBInfo
{
	int mv0;		// forward 16x16 mv / 16x8 field 1st mv
	int mv1;		// backward 16x16 mv / 16x8 field 1st mv
	int mv2;		// forward 16x8 field 2nd mv
	int mv3;		// backward 18x8 field 2nd mv
	uint8   fieldSelect[4];	// corresp to mvs above (only used for field motion type)

	uint8   mbType; // |1 = fwdmotion, |2 = bckmotion, |4 = field motion type (not frame/16x8)
	uint8   interlacedDCT;
	uint8   n[6];
};

struct DCTSpec
{
	uint8   runSubOne;
	uint8   lowOfEltHereLastFrame;		// almost certainly a result of driver swapping shorts itself
	uint16  elt;	// adjusted -0x0400 for first coded elt in intra blocks (don't ask)
};

struct DecodeParams
{
	uint8   pictType;   // 1 = I, 2 = P, 3 = B
	uint8   zero1;
	uint16  sevenSixEight;	// always 768 (not a width)
	
	uint8   alternateScan;
	uint8	zero2;
	uint8   dstBuf;
	uint8   srcBufL;
	
	uint8   srcBufR;
	uint8   zero3[3];
	
	MBInfo  *mbInfo;
	DCTSpec *dctSpecs;
	uint8   *cbp;   // the coded block pattern for each MB
	uint8   *p4;	// guessed to be subpicture data
	CompletionInfo  *completionInfo;
	// after here, setting to 0x00 or 0xFF does not change function
	// (i.e. this is really the end of the structure :)
};

struct CompletionInfo
{
	void *  funcTable;  // virtual?
	uint8   *unk1;
	int		frameNumberToDisplay;
	int		zero1;
	uint8   *frameDataToDisplay;
	int		unk2[3];
	//...
};

struct CallbackTuple
{
	void (*func)( int ret, int arg2 );
	int arg2;
	
	static const CallbackTuple * DONT_RETURN_RESULT() { return (CallbackTuple*)-1; }
	// I think above might have indicated that an async func call was
	// desired... however the only effect at the mo' is not returning the result
};

struct DeinterlaceParams1
{
	uint8   buf1;
	uint8   buf2;
	uint8   zero1[2];
	
	int		w1; // 1
	int		w2; // 1
	int		w3; // 1 or 2 (2 when buf1 == buf2)
};

struct DeinterlaceParams2
{
	int		unk1;		// always seems to be 0x00020000
	int		usedValue;  // 0 or 1. AFAICT only this element is used
	int		unk2;
	int		unk3;
	int		maybe[2];		// might include these bytes too
	//uint8 unlikely[80]	// probably doesn't include these
	// this struct is small enough to be inlined in the caller's context struct
};

struct ShowBufferParams
{
	uint16  h1;		// always seems to be '2'
	uint16  zero1;
	int		w;		// always seems to be '1'
	int		zero2[4];
	// can't help noticing this is very similar to DeinterlaceParams2!
};

#pragma pack()

extern int DVDVideoOpenDevice(
	CGDirectDisplayID cgDisplayID, CGSConnectionID cgSConnectionID, int windowID, CGSSurfaceID surfaceID,
	const short * rectIn, void * othOut, DVDVideoContext ** ppContext );
//extern int DVDDriverOpenDevice( DVDDriverContext ** ppContext,
//	DVDVideoSize * pMaxSize, CGDirectDisplayID cgDisplayID,
//	int cbarg1, int cbarg2, int cbarg3, int * pSomeCount );

extern int DVDVideoCloseDevice( DVDVideoContext * pContext );
//extern int DVDDriverCloseDevice( DVDDriverContext * pContext );

extern int DVDVideoSetMVLevel( DVDVideoContext * pContext, int level );

extern int DVDVideoClearMP( DVDVideoContext * pContext );

extern int DVDVideoSetMPRects( DVDVideoContext * context, short * rect, CGSSurfaceID surfaceID1, CGSSurfaceID surfaceID2 );

extern int DVDVideoEnableMP( DVDVideoContext * pContext, bool enable );

extern int DVDVideoSetFeatureParam( DVDVideoContext * pContext, int feat, int val );
// sets 4->4, then after decoding+deinterlacing first frame, sets 4->0 then 4->4

extern int DVDVideoDecode( DVDVideoContext * pContext,
	DecodeParams * params, short * rect, int unknownZero=0 );

extern int DVDVideoDeinterlace( DVDVideoContext * pContext,
	DeinterlaceParams1 * params1, DeinterlaceParams2 * params2, CallbackTuple * callback );

extern int DVDVideoShowMPBuffer( DVDVideoContext * pContext,
	int whichBuf, ShowBufferParams * params, CallbackTuple * callback );
}

#endif // ACCEL_PRIVATE_H_
