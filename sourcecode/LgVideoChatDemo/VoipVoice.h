#pragma once

typedef struct
{
	bool AecOn;
	bool NoiseSuppressionOn;
}TVoipAttr;

bool VoipVoiceStart(char* hostname, unsigned short localport, unsigned short remoteport, TVoipAttr &VoipAttrRef);
bool VoipVoiceStop(void);
