#pragma once
#define DEBUGFRAMES 80000

#ifdef _WIN32
#define GRAPH_P(X,Y,P) graph[3*(((oheight+30)-(Y))*owidth+(X))+P]
#else
#define GRAPH_P(X,Y,P) graph[3*((Y)*owidth+(X))+P]
#endif

#define GRAPH_R(X,Y) GRAPH_P(X,Y,2)
#define GRAPH_G(X,Y) GRAPH_P(X,Y,1)
#define GRAPH_B(X,Y) GRAPH_P(X,Y,0)

#define PIXEL(X,Y) GRAPH_R(X,Y) = GRAPH_G(X,Y) = GRAPH_B(X,Y)
#define SETPIXEL(X,Y, R,G,B) { GRAPH_R(X,Y) = (R); GRAPH_G(X,Y) = (G); GRAPH_B(X,Y) = (B); }

#define PLOT(S, I, X, Y, MAX, L, R,G,B) { int y, o; o = oheight - (oheight/(S))* (I); y = (Y)*(oheight/(S)-5)/(MAX); if (y < 0) y = 0; if (y > (oheight/(S)-1)) y = (oheight/(S)-1); SETPIXEL((X),(o - y),((Y) < (L) ? 255: R ) , ((Y) < (L) ? 255: G ) ,((Y) < (L) ? 255: B));}


#define LOGOBORDER	4*edge_step
#define LOGO_Y_LOOP	int y_max_test = (subtitles? height/2 : (height - edge_radius - border - LOGOBORDER)); \
                    int y_step_test = height/3; \
                    for (y = (logo_at_bottom ? height/2 : edge_radius + border + LOGOBORDER); y < y_max_test; y = (y==y_step_test ? 2*height/3 : y+edge_step))
// #define LOGO_X_LOOP for (x = max(edge_radius + (int)(width * borderIgnore), minX+AR_DIST); x < min((width - edge_radius - (int)(width * borderIgnore)),maxX-AR_DIST); x += edge_step)
#define LOGO_X_LOOP for (x = (logo_at_side ? width/2 : edge_radius + border + LOGOBORDER); x < (videowidth - edge_radius - border- LOGOBORDER); x = (x==videowidth/3 ? 2*videowidth/3 : x+edge_step))




