/***************************************************************************
 SimpleMail - Copyright (C) 2000 Hynek Schlawack and Sebastian Bauer

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
***************************************************************************/

/**
 * @file datatypescache.h
 */
#ifndef SM__DATATYPESCACHE_H
#define SM__DATATYPESCACHE_H

/* This is more a function belonging to amigasupport.c */
/**
 * Loads the picture and maps it to the given screen. Returns
 * a datatypes object which needs to be freed with
 * DisposeDTObject(). Note the picture cannot be remapped
 * to another screen.
 *
 * @param filename the name of the file that should be loaded
 * @param scr the screen on which the object shall be displayed
 * @return the datatypes object
 */
Object *LoadAndMapPicture(char *filename, struct Screen *scr);

/**
 * Initialize the cache.
 */
void dt_init(void);

/**
 * Free all resources associated with the cache.
 */
void dt_cleanup(void);

/**
 * Load the dt object but don't map it to any screen. The same file (as per
 * filename) is instantiated only once.
 *
 * @param filename the file to be loaded
 * @return the node/object that you can supply to various functions to do nice
 *  things
 */
struct dt_node *dt_load_unmapped_picture(char *filename);

/**
 * Load the dt object and map it to a given screen.. The same file (as per
 * filename) is instantiated only once.
 *
 * @param filename the file to be loaded
 * @param scr the screen to which the loaded object shall be mapped
 * @return the node/object that you can supply to various functions to do nice
 *  things
 */
struct dt_node *dt_load_picture(char *filename, struct Screen *scr);

/**
 * Dispose the given dt
 *
 * @param node the node/object to be disposed
 */
void dt_dispose_picture(struct dt_node *node);

/**
 * Return the width of the object.
 *
 * @param node the object of which the width shall be determined
 *
 * @return the width
 */
int dt_width(struct dt_node *node);

/**
 * Return the height of the object.
 *
 * @param node the object of which the height shall be determined
 *
 * @return the height
 */
int dt_height(struct dt_node *node);

/**
 * Put the node/object into the given rastport.
 *
 * @param node the node to be rendered
 * @param rp the rastport that defines the destination
 * @param x the x offset within the rastport
 * @param y the y offset within the rastport
 */
void dt_put_on_rastport(struct dt_node *node, struct RastPort *rp, int x, int y);

/**
 * Paste the dt node into an ARGB buffer which is ensured
 * to have enough space.
 *
 * @param node the node to be rendered
 * @param dest the dest buffer
 * @param dest_width the width of the dest buffers (pixels per row)
 * @param x the x offset within the dest buffer
 * @param y the y offset within the dest buffer
 */
void dt_put_on_argb(struct dt_node *node, void *dest, int dest_width, int x, int y);

/**
 * Returns the ARGB buffer solely of this image. You can use
 * dt_width() and dt_height() to query the dimension.
 *
 * @param node the node/object of interest
 * @return the ARGB buffer. Free it via FreeVec() when no longer in use.
 */
void *dt_argb(struct dt_node *node);

#endif
