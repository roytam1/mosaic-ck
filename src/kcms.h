#ifdef _KCMS_
int KCMS_Enabled;
int KCMS_Return_Format;
#else
extern int KCMS_Enabled;
extern int KCMS_Return_Format;
#endif

#define JPEG 0
#define JYCC 1
#define GIF 2


void CheckKCMS(void);
