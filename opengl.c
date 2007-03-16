

#include "opengl.h"

#include "SDL.h"
#include "SDL_image.h"

#include "all.h"
#include "log.h"


/* Recommended for compatibility and such */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#  define RMASK	0xff000000
#  define GMASK	0x00ff0000
#  define BMASK	0x0000ff00
#  define AMASK	0x000000ff
#else
#  define RMASK	0x000000ff
#  define GMASK	0x0000ff00
#  define BMASK	0x00ff0000
#  define AMASK	0xff000000
#endif
#define RGBAMASK	RMASK,GMASK,BMASK,AMASK


/* the screen info, gives data of current opengl settings */
gl_info gl_screen;

/* the camera */
Vector2d* gl_camera;


static int flip_surface( SDL_Surface* surface );


/*
 * flips the surface vertically
 *
 * returns 0 on success
 */
static int flip_surface( SDL_Surface* surface )
{
	/* flip the image */
	Uint8 *rowhi, *rowlo, *tmpbuf;
	int y;

	tmpbuf = (Uint8 *)malloc(surface->pitch);
	if ( tmpbuf == NULL ) {
		WARN("Out of memory");
		return -1;
	}

	rowhi = (Uint8 *)surface->pixels;
	rowlo = rowhi + (surface->h * surface->pitch) - surface->pitch;
	for (y = 0; y < surface->h / 2; ++y ) {
		memcpy(tmpbuf, rowhi, surface->pitch);
		memcpy(rowhi, rowlo, surface->pitch);
		memcpy(rowlo, tmpbuf, surface->pitch);
		rowhi += surface->pitch;
		rowlo -= surface->pitch;
	}
	free(tmpbuf);
	/* flipping done */

	return 0;
}


/*
 * loads the image as an opengl texture directly
 */
gl_texture*  gl_newImage( const char* path )
{
	SDL_Surface *temp, *surface;
	Uint32 saved_flags;
	Uint8  saved_alpha;
	int potw, poth;

	temp = IMG_Load( path ); /* loads the surface */
	if (temp == 0) {
		WARN("'%s' could not be opened: %s", path, IMG_GetError());
		return 0;
	}

	surface = SDL_DisplayFormatAlpha( temp ); /* sets the surface to what we use */
	if (surface == 0) {
		WARN( "Error converting image to screen format: %s", SDL_GetError() );
		return 0;
	}

	SDL_FreeSurface( temp ); /* free the temporary surface */

	flip_surface( surface );

	/* set up the texture defaults */
	gl_texture *texture = MALLOC_ONE(gl_texture);
	texture->w = (FP)surface->w;
	texture->h = (FP)surface->h;
	texture->sx = 1.0;
	texture->sy = 1.0;

	/* Make size power of two */
	potw = surface->w;
	if ((potw & (potw - 1)) != 0) {
		potw = 1;
		while (potw < surface->w)
			potw <<= 1;
	} texture->rw = potw;
	poth = surface->h;
	if ((poth & (poth - 1)) != 0) {
		poth = 1;
		while (poth < surface->h)
			poth <<= 1;
	} texture->rh = poth;

	if (surface->w != potw || surface->h != poth ) { /* size isn't original */
		SDL_Rect rtemp;
		rtemp.x = rtemp.y = 0;
		rtemp.w = surface->w;
		rtemp.h = surface->h;

		/* saves alpha */
		saved_flags = surface->flags & (SDL_SRCALPHA | SDL_RLEACCELOK);
		saved_alpha = surface->format->alpha;
		if ( (saved_flags & SDL_SRCALPHA) == SDL_SRCALPHA )
			SDL_SetAlpha( surface, 0, 0 );

		
		/* create the temp POT surface */
		temp = SDL_CreateRGBSurface( SDL_SRCCOLORKEY,
				texture->rw, texture->rh, surface->format->BytesPerPixel*8, RGBAMASK );
		if (temp == NULL) {
			WARN("Unable to create POT surface: %s", SDL_GetError());
			return 0;
		}
		if (SDL_FillRect( temp, NULL, SDL_MapRGBA(surface->format,0,0,0,SDL_ALPHA_TRANSPARENT))) {
			WARN("Unable to fill rect: %s", SDL_GetError());
			return 0;
		}

		SDL_BlitSurface( surface, &rtemp, temp, &rtemp);
		SDL_FreeSurface( surface );

		surface = temp;

		/* set saved alpha */
		if ( (saved_flags & SDL_SRCALPHA) == SDL_SRCALPHA )
			SDL_SetAlpha( surface, saved_flags, saved_alpha );
	}

	glGenTextures( 1, &texture->texture ); /* Creates the texture */
	glBindTexture( GL_TEXTURE_2D, texture->texture ); /* Loads the texture */

	/* Filtering, LINEAR is better for scaling, nearest looks nicer, LINEAR
	 * also seems to create a bit of artifacts around the edges */
/*	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);*/
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	SDL_LockSurface( surface );
	glTexImage2D( GL_TEXTURE_2D, 0, surface->format->BytesPerPixel,
			 surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels );
	SDL_UnlockSurface( surface );

	SDL_FreeSurface( surface );

	return texture;
}


/*
 * Loads the texture immediately, but also sets it as a sprite
 */
gl_texture* gl_newSprite( const char* path, const int sx, const int sy )
{
	gl_texture* texture;
	if ((texture = gl_newImage(path)) == NULL)
		return NULL;
	texture->sx = (FP)sx;
	texture->sy = (FP)sy;
	texture->sw = texture->w/texture->sx;
	texture->sh = texture->h/texture->sy;
	return texture;
}


/*
 * frees the texture
 */
void gl_free( gl_texture* texture )
{
	glDeleteTextures( 1, &texture->texture );
	free(texture);
}


/*
 * blits a sprite at pos
 */
void gl_blitSprite( gl_texture* sprite, Vector2d* pos, const int sx, const int sy )
{
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
		glTranslatef( sprite->sw*(FP)(sx)/sprite->rw,
				sprite->sh*(sprite->sy-(FP)sy-1)/sprite->rh, 0.0f );

	glMatrixMode(GL_PROJECTION);
	glPushMatrix(); /* projection translation matrix */
		glTranslatef( gl_camera->x - pos->x - sprite->sw/2.0,
				gl_camera->y - pos->y - sprite->sh/2.0, 0.0f);

	/* actual blitting */
	glBindTexture( GL_TEXTURE_2D, sprite->texture);
	glBegin( GL_TRIANGLE_STRIP );
		glTexCoord2f( 0.0f, 0.0f);
			glVertex2f( 0.0f, 0.0f );
		glTexCoord2f( sprite->sw/sprite->rw, 0.0f);
			glVertex2f( sprite->sw, 0.0f );
		glTexCoord2f( 0.0f, sprite->sh/sprite->rh);
			glVertex2f( 0.0f, sprite->sh );
		glTexCoord2f( sprite->sw/sprite->rw, sprite->sh/sprite->rh);
			glVertex2f( sprite->sw, sprite->sh );
	glEnd();

	glPopMatrix(); /* projection translation matrix */

	glMatrixMode(GL_TEXTURE);
	glPopMatrix(); /* sprite translation matrix */
}


/*
 * straight out blits a texture at position
 */
void gl_blit( gl_texture* texture, Vector2d* pos )
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix(); /* set up translation matrix */
		glTranslatef( pos->x, pos->y, 0);

	/* actual blitting */
	glBindTexture( GL_TEXTURE_2D, texture->texture);
	glBegin( GL_TRIANGLE_STRIP );
		glTexCoord2f( 0.0f, 0.0f);
			glVertex2f( 0.0f, 0.0f );
		glTexCoord2f( texture->w/texture->rw, 0.0f);
			glVertex2f( texture->w, 0.0f );
		glTexCoord2f( 0.0f, texture->h/texture->rh);
			glVertex2f( 0.0f, texture->h );
		glTexCoord2f( texture->w/texture->rw, texture->h/texture->rh);
			glVertex2f( texture->w, texture->h );
	glEnd();

	glPopMatrix(); /* pop the translation matrix */
}


/*
 * Binds the camero to a vector
 */
void gl_bindCamera( Vector2d* pos )
{
	gl_camera = pos;
}


/*
 * Initializes SDL/OpenGL and the works
 */
int gl_init()
{
	int depth;
	int flags = SDL_OPENGL;

	/* Initializes Video */
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		WARN("Unable to initialize SDL: %s", SDL_GetError());
		return -1;
	}

	/* we don't want none of that ugly cursor */
	SDL_ShowCursor(SDL_DISABLE);

	flags |= SDL_FULLSCREEN * gl_screen.fullscreen;
	depth = SDL_VideoModeOK( gl_screen.w, gl_screen.h, gl_screen.depth, flags); /* test set up */
	if (depth != gl_screen.depth)
		WARN("Depth %d bpp unavailable, will use %d bpp", gl_screen.depth, depth);

	gl_screen.depth = depth;

	/* actually creating the screen */
	if (SDL_SetVideoMode( gl_screen.w, gl_screen.h, gl_screen.depth, flags) == NULL) {
		WARN("Unable to create OpenGL window: %s", SDL_GetError());
		SDL_Quit();
		return -1;
	}

	/* Get info about the OpenGL window */
	SDL_GL_GetAttribute( SDL_GL_RED_SIZE, &gl_screen.r );
	SDL_GL_GetAttribute( SDL_GL_GREEN_SIZE, &gl_screen.g );
	SDL_GL_GetAttribute( SDL_GL_BLUE_SIZE, &gl_screen.b );
	SDL_GL_GetAttribute( SDL_GL_ALPHA_SIZE, &gl_screen.a );
	SDL_GL_GetAttribute( SDL_GL_DOUBLEBUFFER, &gl_screen.doublebuf );
	gl_screen.depth = gl_screen.r + gl_screen.g + gl_screen.b + gl_screen.a;

	/* Debug happiness */
	DEBUG("OpenGL Window Created: %dx%d@%dbpp %s", gl_screen.w, gl_screen.h, gl_screen.depth,
			gl_screen.fullscreen?"fullscreen":"window");
	DEBUG("r: %d, g: %d, b: %d, a: %d, doublebuffer: %s",
			gl_screen.r, gl_screen.g, gl_screen.b, gl_screen.a, gl_screen.doublebuf?"yes":"no");
	DEBUG("Renderer: %s", glGetString(GL_RENDERER));

	/* some OpenGL options */
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glDisable( GL_DEPTH_TEST ); /* set for doing 2d */
	glEnable( GL_TEXTURE_2D );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( -gl_screen.w/2, /* left edge */
			gl_screen.w/2, /* right edge */
			-gl_screen.h/2, /* bottom edge */
			gl_screen.h/2, /* top edge */
			-1.0f, /* near */
			1.0f ); /* far */
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ); /* alpha */
	glEnable( GL_BLEND );

	glClear( GL_COLOR_BUFFER_BIT );


	SDL_WM_SetCaption( WINDOW_CAPTION, NULL );

	return 0;

}


/*
 * Cleans up SDL/OpenGL, the works
 */
void gl_exit()
{
	SDL_ShowCursor(SDL_ENABLE);
	SDL_Quit();
}
