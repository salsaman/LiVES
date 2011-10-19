// colourspace.h
// LiVES
// (c) G. Finch 2004 - 2010 <salsaman@xs4all.nl>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

// headers for palette conversions

typedef struct {
  guchar u0;
  guchar y0;
  guchar v0;
  guchar y1;
} uyvy_macropixel;

typedef struct {
  guchar y0;
  guchar u0;
  guchar y1;
  guchar v0;
} yuyv_macropixel;


typedef struct {
  guchar u2;
  guchar y0;
  guchar y1;
  guchar v2;
  guchar y2;
  guchar y3;
} yuv411_macropixel;


///////////////////////////////////////
// these functions should be used in future
gboolean convert_layer_palette(weed_plant_t *layer, int outpl, int op_clamping);
gboolean convert_layer_palette_with_sampling(weed_plant_t *layer, int outpl, int out_sampling);
gboolean apply_gamma (weed_plant_t *ilayer, weed_plant_t *olayer, double gamma); ///< not used
void resize_layer (weed_plant_t *layer, int width, int height, int interp);
void letterbox_layer (weed_plant_t *layer, int width, int height, int nwidth, int nheight);
void create_empty_pixel_data(weed_plant_t *layer, gboolean black_fill);
void pixel_data_planar_from_membuf(void **pixel_data, void *data, size_t size, int palette);
GdkPixbuf *layer_to_pixbuf (weed_plant_t *layer);
gboolean pixbuf_to_layer(weed_plant_t *layer, GdkPixbuf *);

weed_plant_t *weed_layer_copy (weed_plant_t *dlayer, weed_plant_t *slayer);
void weed_layer_free (weed_plant_t *layer);
weed_plant_t *weed_layer_new(int width, int height, int *rowstrides, int current_palette);
int weed_layer_get_palette(weed_plant_t *layer);

// palette information functions
gboolean weed_palette_is_valid_palette(int pal);
gboolean weed_palette_is_alpha_palette(int pal);
gboolean weed_palette_is_rgb_palette(int pal);
gboolean weed_palette_is_yuv_palette(int pal);
gboolean weed_palette_is_float_palette(int pal);
gboolean weed_palette_has_alpha_channel(int pal);
gint weed_palette_get_bits_per_macropixel(int pal);
gint weed_palette_get_pixels_per_macropixel(int pal);
gint weed_palette_get_numplanes(int pal);
gdouble weed_palette_get_plane_ratio_horizontal(int pal, int plane);
gdouble weed_palette_get_plane_ratio_vertical(int pal, int plane);
gboolean weed_palette_is_lower_quality(int p1, int p2);  ///< return TRUE if p1 is lower quality than p2
gboolean weed_palette_is_resizable(int pal);
gdouble weed_palette_get_compression_ratio (int pal);

int fourccp_to_weedp (unsigned int fourcc, int bpp, lives_interlace_t *interlace, int *sampling, 
		      int *sspace, int *clamping);

#define BLACK_THRESH 20 ///< if R,G and B values are all <= this, we consider it a "black" pixel
gboolean gdk_pixbuf_is_all_black(GdkPixbuf *pixbuf);


void gdk_pixbuf_set_opaque(GdkPixbuf *pixbuf);

const char *weed_palette_get_name(int pal);
const char *weed_yuv_clamping_get_name(int clamping);
const char *weed_yuv_subspace_get_name(int subspace);
gchar *weed_palette_get_name_full(int pal, int clamped, int subspace);

