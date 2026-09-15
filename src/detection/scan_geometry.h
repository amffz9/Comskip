#pragma once
#define DEBUGFRAMES 80000

#ifdef _WIN32
#define GRAPH_P(X,Y,P) context.state.graph[3*(((context.state.oheight+30)-(Y))*context.state.owidth+(X))+P]
#else
#define GRAPH_P(X,Y,P) context.state.graph[3*((Y)*context.state.owidth+(X))+P]
#endif

#define GRAPH_R(X,Y) GRAPH_P(X,Y,2)
#define GRAPH_G(X,Y) GRAPH_P(X,Y,1)
#define GRAPH_B(X,Y) GRAPH_P(X,Y,0)

#define PIXEL(X,Y) GRAPH_R(X,Y) = GRAPH_G(X,Y) = GRAPH_B(X,Y)
#define SETPIXEL(X,Y, R,G,B) { GRAPH_R(X,Y) = (R); GRAPH_G(X,Y) = (G); GRAPH_B(X,Y) = (B); }

#define PLOT(S, I, X, Y, MAX, L, R,G,B) { int y, o; o = context.state.oheight - (context.state.oheight/(S))* (I); y = (Y)*(context.state.oheight/(S)-5)/(MAX); if (y < 0) y = 0; if (y > (context.state.oheight/(S)-1)) y = (context.state.oheight/(S)-1); SETPIXEL((X),(o - y),((Y) < (L) ? 255: R ) , ((Y) < (L) ? 255: G ) ,((Y) < (L) ? 255: B));}


#define LOGOBORDER	4*context.settings.edge_step
#define LOGO_Y_LOOP	int y_max_test = (context.settings.subtitles? context.state.height/2 : (context.state.height - context.settings.edge_radius - context.settings.border - LOGOBORDER)); \
                    int y_step_test = context.state.height/3; \
                    for (y = (context.settings.logo_at_bottom ? context.state.height/2 : context.settings.edge_radius + context.settings.border + LOGOBORDER); y < y_max_test; y = (y==y_step_test ? 2*context.state.height/3 : y+context.settings.edge_step))
// #define LOGO_X_LOOP for (x = max(context.settings.edge_radius + (int)(context.state.width * borderIgnore), minX+AR_DIST); x < min((context.state.width - context.settings.edge_radius - (int)(context.state.width * borderIgnore)),maxX-AR_DIST); x += context.settings.edge_step)
#define LOGO_X_LOOP for (x = (context.settings.logo_at_side ? context.state.width/2 : context.settings.edge_radius + context.settings.border + LOGOBORDER); x < (context.state.videowidth - context.settings.edge_radius - context.settings.border- LOGOBORDER); x = (x==context.state.videowidth/3 ? 2*context.state.videowidth/3 : x+context.settings.edge_step))




