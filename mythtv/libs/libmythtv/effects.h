void linearBlendYUV420(unsigned char *yuvptr, int width, int height);

void InitializeOSD(int width, int height);
void DisplayOSD(unsigned char *yuvptr);

void SetOSDInfoText(char *text, int length);
void SetOSDChannumText(char *text, int length);
