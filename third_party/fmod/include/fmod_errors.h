#ifndef _FMOD_ERRORS_H_
#define _FMOD_ERRORS_H_
//
// Clean-room companion to fmod.h. The engine only feeds FMOD_ErrorString's
// result straight to OutputDebugString for diagnostics, so a compact mapping is
// sufficient -- the engine never depends on the exact wording. Not a DLL export
// (the vendor header inlines it too); reconstructed from observed FMOD 3 codes.
//

#ifdef __cplusplus
extern "C" {
#endif

static const char *FMOD_ErrorString( int errcode )
{
	switch ( errcode )
	{
		case 0:  return "No errors";
		case 1:  return "Cannot call this command after FSOUND_Init.  Call FSOUND_Close first.";
		case 2:  return "This command failed because FSOUND_Init was not called";
		case 3:  return "Error initializing output device";
		case 4:  return "Error initializing output device, but more specifically, the output device is already in use and cannot be reused.";
		case 5:  return "Playing the sound failed";
		case 6:  return "Soundcard does not support the features needed for this soundsystem (16bit stereo output)";
		case 7:  return "Error setting cooperative level for hardware";
		case 8:  return "Error creating hardware sound buffer";
		case 9:  return "File not found";
		case 10: return "Unknown file format";
		case 11: return "Error loading file";
		case 12: return "Not enough memory";
		case 13: return "The version number of this file format is not supported";
		case 14: return "An invalid parameter was passed to this function";
		case 15: return "Tried to use an EAX command on a non EAX enabled channel or output";
		case 16: return "Failed to allocate a new channel";
		case 17: return "Recording is not supported on this machine";
		case 18: return "Error trying to allocate a channel";
		default: return "Unknown error";
	}
}

#ifdef __cplusplus
}
#endif

#endif
