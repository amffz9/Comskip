#ifndef _VO_H
#define _VO_H
#ifdef __cplusplus
extern "C" {
#endif
void vo_init(int width, int height, char *title);
void vo_draw(unsigned char * buf);
void vo_refresh();
void vo_wait();
void vo_close();
void ShowHelp(const char *const *ta);
void ShowDetails(char *t);
#ifdef __cplusplus
}
#endif
#endif
