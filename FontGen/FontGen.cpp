#include "stdafx.h"

#include "..\Image\image.h"
#include "..\Image\imageTGA.h"
#include "..\Misc\StrProc.h"
#include "FontFormat.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_LEADING_PIXELS = 2;
////////////////////////////////////////////////////////////////////////////////////////////////////
HWND hWnd;
HINSTANCE hInst;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFontInfo
//      This class stores information about the currently loaded font.
//      This includes LOGFONT and CHOOSEFONT structures for use with the
//      ChooseFont dialog, as well as info about character dimensions.
//
struct SFontInfo
{
  HFONT hFont;                // HFONT used to draw with this font
  TEXTMETRIC tm;              // text metrics, e.g. character height
	std::vector<ABC> abc;									// character ABC widths
	std::vector<KERNINGPAIR> kps;					// kernging pairs
	int nTextureSizeX, nTextureSizeY;			// estimated texture size
	std::unordered_map<WORD, WORD> translate;	// ANSI => UNICODE translation table
	//
	WORD Translate( WORD code ) const
	{
		std::unordered_map<WORD, WORD>::const_iterator pos = translate.find( code );
		ASSERT( pos != translate.end() );
		if ( pos == translate.end() )
		{
			DebugTrace( "Can't find code for symbol %d to re-map", code );
			return -1;
		}
		return pos->second;
	}
	//
	SFontInfo() : hFont( 0 ), nTextureSizeX( 0 ), nTextureSizeY( 0 ) {  }
	virtual ~SFontInfo() { if ( hFont ) DeleteObject( hFont ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// estimate, is requested number of chars fit in the selected texture
inline bool IsFit( const SFontInfo &fi, DWORD dwNumChars, DWORD dwSizeX, DWORD dwSizeY )
{
  return ( dwSizeX / (fi.tm.tmAveCharWidth + 2) ) * ( dwSizeY / fi.tm.tmHeight ) >= dwNumChars;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool EstimateTextureSize( SFontInfo *pFI, DWORD dwNumChars )
{
	SFontInfo &fi = *pFI;
  for ( int i=6; i<13; ++i )
  {
    // first, try to estimate 2:1 size
    if ( IsFit( fi, dwNumChars, 1 << i, 1 << (i - 1) ) )
    {
			fi.nTextureSizeX = 1 << i;
			fi.nTextureSizeY = 1 << (i - 1);
      return true;
    }
    // then, try to estimate 1:1 size
    else if ( IsFit( fi, dwNumChars, 1 << i, 1 << i ) )
    {
      fi.nTextureSizeX = fi.nTextureSizeY = 1 << i;
      return true;
    }
  }
  // too big texture!!!
  return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SKPZeroFunctional
{
  bool operator()( const KERNINGPAIR &kp ) const { return kp.iKernAmount == 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//      Fills CFontInfo fi (global) with text metrics and char widths
//      -> hdc: HDC that the font is currently selected into
//
void MeasureFont( HDC hdc, SFontInfo *pFI, vector<WORD> *pChars )
{
	vector<WORD> &chars = *pChars;
	SFontInfo &fi = *pFI;
  GetTextMetrics( hdc, &fi.tm );
	std::sort( chars.begin(), chars.end() );
	if ( std::find( chars.begin(), chars.end(), fi.tm.tmDefaultChar ) == chars.end() )
		chars.push_back( fi.tm.tmDefaultChar );
  // Measure TrueType fonts with GetCharABCWidths:
	fi.abc.resize( chars.size() );
	if ( !GetCharABCWidths( hdc, chars[0], chars[0], &( fi.abc[0] ) ) )
	{
		// 
		ABC abc;
		Zero( abc );
		std::fill( fi.abc.begin(), fi.abc.end(), abc );
		// If it's not a TT font, use GetTextExtentPoint32 to fill array abc:
		SIZE size;
		for ( int i=0; i<chars.size(); ++i )
		{
			// get width of character...
			GetTextExtentPoint32( hdc, (const char*)&( chars[i] ), 1, &size );
			// ...and store it in abcB:
			fi.abc[i].abcB = size.cx;
		}
	}
	else
	{
		for ( int i=0; i<chars.size(); ++i )
			GetCharABCWidths( hdc, chars[i], chars[i], &( fi.abc[i] ) );
	}
  // get kerning pairs
	KERNINGPAIR kernpair;
	Zero( kernpair );
	fi.kps.resize(0);
	fi.kps.resize( chars.size() * chars.size(), kernpair );
	GetKerningPairs( hdc, chars.size()*chars.size(), &( fi.kps[0] ) );
  // remove kerning pairs with '0' kern value
  fi.kps.erase( std::remove_if( fi.kps.begin(), fi.kps.end(), SKPZeroFunctional() ), fi.kps.end() );

  // estimate texture size
  if ( !EstimateTextureSize( &fi, chars.size() ) )
    throw 1; // too large texture !!!
  // check and correct size estimating
  int x = 0, y = 0;
	for ( int i=0; i<chars.size(); ++i )
	{
    int nNextCharShift = fi.abc[i].abcB + abs( fi.abc[i].abcC );
    if ( x + nNextCharShift + N_LEADING_PIXELS > fi.nTextureSizeX )
    {
      ++y;
      x = 0;
      if ( (y + 1) * fi.tm.tmHeight > fi.nTextureSizeY )
      {
        if ( fi.nTextureSizeX == fi.nTextureSizeY ) // if we have 1:1 sizes, make it 2:1
          fi.nTextureSizeX <<= 1;
        else                                   // else, if we have 2:1 already, make it 2:2 :)
          fi.nTextureSizeY = fi.nTextureSizeX;
        break;
      }
    }
    x += N_LEADING_PIXELS;
    x += nNextCharShift;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void LoadFont( HWND hWnd, SFontInfo *pFI, int nHeight, int nWeight, bool bItalic, DWORD dwCharSet, 
	bool bAntialias, DWORD dwPitch, const std::string &szFaceName, std::vector<WORD> *pChars )
{
	SFontInfo &fi = *pFI;
	std::vector<WORD> &chars = *pChars;
  // invoke ChooseFont common dialog:
  // create an HFONT:
  if ( fi.hFont )
  { 
    DeleteObject( fi.hFont ); 
    fi.hFont = 0;
  }
  fi.hFont = ::CreateFont( nHeight, 0, 0, 0, nWeight, bItalic, FALSE, FALSE, 
                           dwCharSet, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                           bAntialias ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY,
                           dwPitch, szFaceName.c_str() );
  // retrieve logfont
//  ::GetObject( fi.hFont, sizeof(fi.lf), &fi.lf );
  // get HDC:
  HDC hdc = GetDC( hWnd );
  // select font:
  HFONT hOldFont = (HFONT)::SelectObject( hdc, fi.hFont );
	//
  // get text metrics and char widths:
  MeasureFont( hdc, &fi, &chars );
	// translate chars to UNICODE and re-map kerns and chars
	{
		CHARSETINFO cs;
		BOOL bRetVal = TranslateCharsetInfo( (DWORD*)dwCharSet, &cs, TCI_SRCCHARSET );
		ASSERT( bRetVal == TRUE );
		NStr::SetCodePage( cs.ciACP );
		// form string
		std::string szCharacters;
		szCharacters.resize( chars.size() );
		for ( int i = 0; i != chars.size(); ++i )
			szCharacters[i] = chars[i];
		std::wstring szUNICODE;
		NStr::ToUnicode( &szUNICODE, szCharacters );
		// create re-map table
		for ( int i = 0; i != chars.size(); ++i )
			fi.translate[ chars[i] ]= szUNICODE[i];
	}
  // select old font
  ::SelectObject( hdc, hOldFont );
  // release HDC:
  ReleaseDC( hWnd, hdc );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// draw font in the DC
bool DrawFont( HDC hdc, const SFontInfo &fi, const std::vector<WORD> &chars )
{
  // Draw characters:
  int x = 0, y = 0;
	for ( int i=0; i<chars.size(); ++i )
	{
		WORD wChar = chars[i];
		int nNextCharShift = fi.abc[i].abcB + abs( fi.abc[i].abcC );
		if ( x + nNextCharShift + N_LEADING_PIXELS > fi.nTextureSizeX )
    {
      ++y;
      x = 0;
      if ( (y + 1) * fi.tm.tmHeight > fi.nTextureSizeY )
        return false;
    }
    x += N_LEADING_PIXELS;
		TextOut( hdc, x - fi.abc[i].abcA, y*fi.tm.tmHeight, (const char*)&( chars[i] ), 1 );
    x += nNextCharShift;
  }
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateFontImage( const SFontInfo &fi, NImage::CImage *pRes, const std::vector<WORD> &chars )
{
  // Create an offscreen bitmap:
  int width = fi.nTextureSizeX;//16 * fi.tm.tmMaxCharWidth;
  int height = fi.nTextureSizeY;//14 * fi.tm.tmHeight;
  // Prepare to create a bitmap
  BYTE *pBitmapBits = 0;
  BITMAPINFO bmi;
  memset( &bmi.bmiHeader, 0, sizeof(bmi.bmiHeader) );
  bmi.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER );
  bmi.bmiHeader.biWidth       = fi.nTextureSizeX;
  bmi.bmiHeader.biHeight      = fi.nTextureSizeY;
  bmi.bmiHeader.biPlanes      = 1;
  bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biBitCount    = 24;
  bmi.bmiHeader.biSizeImage   = abs( bmi.bmiHeader.biWidth * bmi.bmiHeader.biHeight * bmi.bmiHeader.biBitCount / 8 );
  // Create a DC and a bitmap for the font
  HDC hDC = CreateCompatibleDC( 0 );
  HBITMAP hbmBitmap = CreateDIBSection( hDC, &bmi, DIB_RGB_COLORS, (void**)&pBitmapBits, 0, 0 );
  HBITMAP hOldBmp = (HBITMAP)SelectObject( hDC, hbmBitmap );
  HFONT hOldFont = (HFONT)SelectObject( hDC, fi.hFont );
  // Clear background to black:
  SelectObject( hDC, GetStockObject(BLACK_BRUSH) );
  Rectangle( hDC, 0, 0, width, height );
  SetBkMode( hDC, TRANSPARENT );           // do not fill character background
  SetTextColor( hDC, RGB(255, 255, 255) ); // text color white
  SetTextAlign( hDC, TA_TOP );
  // Draw characters:
  DrawFont( hDC, fi, chars );
  //
  SelectObject( hDC, hOldFont );
  SelectObject( hDC, hOldBmp );
  //
  // create image
	pRes->SetSizes( fi.nTextureSizeX, fi.nTextureSizeY );
  for ( int i=0, j=0; i< fi.nTextureSizeX * fi.nTextureSizeY * 3; i+=3, ++j )
  {
    //DWORD b = pBitmapBits[i + 0];
    DWORD g = pBitmapBits[i + 1];
    //DWORD r = pBitmapBits[i + 2];
		(*pRes)[ j / fi.nTextureSizeX ][ j % fi.nTextureSizeX ] = CVec4( 1,1,1, g / 255.0f );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFontGen
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFontGen
{
public:
	static void CreateFontFormat( const char *pszDestFile, const SFontInfo &fi, const std::vector<WORD> &chars );
};
void CFontGen::CreateFontFormat( const char *pszDestFile, const SFontInfo &fi, const std::vector<WORD> &chars )
{
	const TEXTMETRIC &tm = fi.tm;
	// textmetric and ABCs must be converted to the next data
  //   1. header data
  //   2. all characters
  //   3. all kerning pairs
  // fill texture font header
	CObj<CFontFormatInfo> pFormat( new CFontFormatInfo );
	CFontFormatInfo &format = *pFormat;
	format.nHeight          = tm.tmHeight;
	format.nExternalLeading = tm.tmExternalLeading;
  format.nAveCharWidth    = tm.tmAveCharWidth;
  format.nMaxCharWidth    = tm.tmMaxCharWidth;
  format.cCharSet         = tm.tmCharSet;
	format.wDefaultChar     = tm.tmDefaultChar;
  // kerning pairs
  //std::vector<SKerningPair> kerns( dwNumKerningPairs );
	for ( int i=0; i<fi.kps.size(); ++i )
	{
		DWORD dwFirst = fi.Translate( fi.kps[i].wFirst );
		DWORD dwSecond = fi.Translate( fi.kps[i].wSecond );
		format.kerns[(dwFirst << 16) | dwSecond] = fi.kps[i].iKernAmount;
	}
  // convert this structures to the STFLetterFull array
  int x = 0, y = 0;
	for ( int i=0; i<chars.size(); ++i )
	{
		BYTE ansicode = chars[i];
		WORD unicode = fi.Translate( chars[i] );
		//
		int nNextCharShift = fi.abc[i].abcB + abs( fi.abc[i].abcC );
		if ( x + nNextCharShift + N_LEADING_PIXELS > fi.nTextureSizeX )
		{
			++y;
			x = 0;
		}
		x += N_LEADING_PIXELS;

		STFCharacter &character = format.chars[unicode];
		// char ABC parameters in the texture's respective size
		character.nA = fi.abc[i].abcA;
		character.nBC = fi.abc[i].abcB + fi.abc[i].abcC;
		character.nWidth = fi.abc[i].abcB + ( fi.abc[i].abcC > 0 ? fi.abc[i].abcC : 0 );
		// character rect in the texture's coords
		character.x1 = x;
		character.y1 = y * tm.tmHeight;
		character.x2 = x + character.nWidth;
		character.y2 = ( y + 1 ) * tm.tmHeight;
		//
		x += nNextCharShift;
	}
	//
	try
	{
		CFileStream file;
		file.OpenWrite( pszDestFile );
		CStructureSaver saver( file, CStructureSaver::WRITE );
		saver.Add( 1, &pFormat );
	}
	catch(...)
	{
		printf( "failed to write %s/n", pszDestFile );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// params:
//   height (in pixels)
//   weight (100-900. normal == 400, bold == 700)
//   italic (t/f)
//   charset
//   antialiased (t/f)
//   pitch (default, fixed, variable)
//   face name (ZB "Times New Roman")

// ANSI_CHARSET
// BALTIC_CHARSET
// CHINESEBIG5_CHARSET
// DEFAULT_CHARSET
// EASTEUROPE_CHARSET
// GB2312_CHARSET
// GREEK_CHARSET
// HANGUL_CHARSET
// MAC_CHARSET
// OEM_CHARSET
// RUSSIAN_CHARSET
// SHIFTJIS_CHARSET
// SYMBOL_CHARSET
// TURKISH_CHARSET
// Windows NT/2000 or Middle-Eastern Windows 3.1 or later: 
// HEBREW_CHARSET
// ARABIC_CHARSET 
// Windows NT/2000 or Thai Windows 3.1 or later: 
// THAI_CHARSET 

static void ShowUsage()
{
	printf( "FontGenerator utility\n(C) Nival Interactive, 2000\n" );
	printf( "Usage: FontGen.exe [options] <\"Font Face Name\"> <DstName> <DstPngName>\n" );
	printf( "   -h# \t\t font height (in pixels)\n" );
	printf( "   -w# \t\t font weight (400 = normal. 100 <= w <= 900)\n" );
	printf( "   -it \t\t italic\n" );
	printf( "   -aa \t\t antialiased quality\n" );
	printf( "   -pitch \t font pitch (default, fixed, variable)\n" );
	printf( "   -<charset>\t second character set\n" );
	printf( "    charsets: ansi, baltic, chinesebig5, default, easteurope, gb2312,\n" );
	printf( "              greek, hangul, mac, oem, russian, shiftjis, symbol,\n" );
	printf( "              turkish, hebrew, arabic, thai\n" );
}

int __cdecl main( int argc, char *argv[] )
{
  // prepare command line
  std::vector<std::string> szParams( argc - 1 );
  for ( int i=0; i<argc - 1; ++i )
  {
    szParams[i] = argv[i + 1];
    NStr::ToLower( szParams[i] );
  }
  //
  if ( szParams.empty() )
  {
		ShowUsage();
    return 0xDEAD;
  }
  // initialize charsets map
  std::unordered_map<std::string, DWORD> charsets;
  charsets["-ansi"]        = ANSI_CHARSET;
  charsets["-baltic"]      = BALTIC_CHARSET;
  charsets["-chinesebig5"] = CHINESEBIG5_CHARSET;
  charsets["-default"]     = DEFAULT_CHARSET;
  charsets["-easteurope"]  = EASTEUROPE_CHARSET;
  charsets["-gb2312"]      = GB2312_CHARSET;
  charsets["-greek"]       = GREEK_CHARSET;
  charsets["-hangul"]      = HANGUL_CHARSET;
  charsets["-mac"]         = MAC_CHARSET;
  charsets["-oem"]         = OEM_CHARSET;
  charsets["-russian"]     = RUSSIAN_CHARSET;
  charsets["-shiftjis"]    = SHIFTJIS_CHARSET;
  charsets["-symbol"]      = SYMBOL_CHARSET;
  charsets["-turkish"]     = TURKISH_CHARSET;
  charsets["-hebrew"]      = HEBREW_CHARSET;
  charsets["-arabic"]      = ARABIC_CHARSET;
  charsets["-thai"]        = THAI_CHARSET;
  // initialize pitch map
  std::unordered_map<std::string, DWORD> pitches;
  pitches["-default"]  = DEFAULT_PITCH;
  pitches["-fixed"]    = FIXED_PITCH;
  pitches["-variable"] = VARIABLE_PITCH;
  // read default values
  char buffer[1024];
  GetModuleFileName( 0, buffer, 1024 );
  std::string szString = buffer;
  szString.erase( szString.find_last_of( '\\' ) );
  szString += "\\fontgen.ini";

  DWORD dwHeight = 20;
  DWORD dwWeight = 400;
  bool bItalic = 0;
  bool bAntialias = 0;
  // pitch
  DWORD dwPitch = VARIABLE_PITCH;
  // charset
  DWORD dwCharSet = ANSI_CHARSET;//DEFAULT_CHARSET;
  // font face name
  std::string szFaceName = "Times New Roman", szDstFile, szDstPngFile;
	int nOrdinaryParamCount = 0;
  // -h20 -w400 -it -russian -aa -variable "Times New Roman"
  for ( std::vector<std::string>::const_iterator pos = szParams.begin(); pos != szParams.end(); ++pos )
  {
    if ( charsets.find(*pos) != charsets.end() )
      dwCharSet = charsets[*pos];
    else if ( pitches.find(*pos) != pitches.end() )
      dwPitch = pitches[*pos];
    else if ( pos->find( "-h" ) == 0 )
      dwHeight = atoi( &((*pos)[2]) );
    else if ( pos->find( "-w" ) == 0 )
      dwWeight = atoi( &((*pos)[2]) );
    else if ( *pos == "-it" )
      bItalic = true;
    else if ( *pos == "-aa" )
      bAntialias = true;
    else
		{
			nOrdinaryParamCount++;
			switch( nOrdinaryParamCount )
			{
				case 1: szFaceName = *pos; break;
				case 2: szDstFile = *pos; break;
				case 3: szDstPngFile = *pos; break;
			}
		}
  }
	if ( nOrdinaryParamCount != 3 )
	{
		ShowUsage();
    return 0xDEAD;
	}
  //
  NStr::TrimInside( szFaceName, '"' );
  //
  printf( "generating font \"%s\" (%d:%d:%d:%d)\n", szFaceName.c_str(), dwHeight, dwWeight, bItalic, bAntialias );
  printf( "image...\n" );
  //
  hWnd = GetDesktopWindow();

	std::vector<WORD> chars;
	chars.reserve( 256 );
	// load font
	for ( int i=32; i<256; ++i ) 
		chars.push_back( i );
	SFontInfo fi;
	LoadFont( GetDesktopWindow(), &fi, dwHeight, dwWeight, bItalic, dwCharSet, bAntialias, dwPitch, szFaceName, &chars );
	// create font image and font data
	NImage::CImage image;
	CreateFontImage( fi, &image, chars );
	{
		CFileStream f;
		f.OpenWrite( szDstPngFile.c_str() );
		NImage::SaveImageAsTGA( &f, image );
	}

	printf( "font data...\n" );
	CFontGen::CreateFontFormat( szDstFile.c_str(), fi, chars );
  printf( "well done\n" );

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
