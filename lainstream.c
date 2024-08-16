#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/vec2.h>
#include <graphics/vec3.h>
#include <graphics/math-extra.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("lainstream", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Lain's custom plugin";
}

/* ========================================================================= */

#define NUM_DARK 100
#define NUM_MID 40
#define NUM_LIGHT 20

#define CX 1920
#define CY 1080
#define F_CX 1920.0f
#define F_CY 1080.0f

struct lain_underlay {
	obs_source_t *source;

	struct vec2 dark_stars[NUM_DARK];
	struct vec2 mid_stars[NUM_MID];
	struct vec2 light_stars[NUM_LIGHT];

	float dark_variations[NUM_DARK];
	float mid_variations[NUM_MID];
	float light_variations[NUM_LIGHT];

	gs_vertbuffer_t *vb;
	gs_texture_t *star;
	gs_texture_t *starfighter;
	gs_samplerstate_t *nearest_override;
	gs_texture_t *trans_flag;
	gs_texture_t *lesbian_flag;
	gs_effect_t *flag_effect;

	float starfighter_offset_deg;
	float starfighter_offset_y;
	float flag_pulse_offset_deg;

	bool show_ship;
};

static const char *lain_underlay_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Lain's Underlay";
}

static void lain_underlay_video_tick(void *data, float seconds);

static inline void vec2_rand(struct vec2 *dst, bool positive_only)
{
	dst->x = rand_float(positive_only);
	dst->y = rand_float(positive_only);
}

static void lain_underlay_update(void *data, obs_data_t *settings)
{
	struct lain_underlay *ul = data;
	ul->show_ship = obs_data_get_bool(settings, "ship");
}

static void *lain_underlay_create(obs_data_t *settings, obs_source_t *source)
{
	struct lain_underlay *ul = bzalloc(sizeof(*ul));
	struct gs_vb_data *vbd;

	ul->source = source;

	/* ---------------------------------- */
	/* create star texture                */

#define TEX_SIZE 16

	uint32_t pixels[TEX_SIZE * TEX_SIZE];
	for (size_t y = 0; y < TEX_SIZE; y++) {
		size_t y_offset = y * TEX_SIZE;

		for (size_t x = 0; x < TEX_SIZE; x++) {
			size_t offset = y_offset + x;

			struct vec2 pos = {
				((float)x + 0.5f) / (float)TEX_SIZE - 0.5f,
				((float)y + 0.5f) / (float)TEX_SIZE - 0.5f};

			vec2_mulf(&pos, &pos, 2.0f);

			float len = vec2_len(&pos);
			if (len > 1.0f)
				len = 1.0f;
			len = 1.0f - len;
			len = powf(len, 5.0);
			uint8_t val = (uint8_t)(len * 255.0f);
			pixels[offset] = val | val << 8 | val << 16 |
					 0xFF000000;
		}
	}

	obs_enter_graphics();
	const uint8_t *ptr = (uint8_t *)pixels;
	ul->star = gs_texture_create(TEX_SIZE, TEX_SIZE, GS_RGBA, 1, &ptr, 0);

#undef TEX_SIZE

	/* ---------------------------------- */
	/* create simple quad                 */

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(1.0f, 0.0f);
	gs_vertex2f(0.0f, 1.0f);
	gs_vertex2f(1.0f, 1.0f);
	gs_texcoord(0.0f, 0.0f, 0);
	gs_texcoord(1.0f, 0.0f, 0);
	gs_texcoord(0.0f, 1.0f, 0);
	gs_texcoord(1.0f, 1.0f, 0);
	ul->vb = gs_render_save();

	/* ---------------------------------- */
	/* create starfighter and sampler     */

	char *path = obs_module_file("starfighter-side.png");
	ul->starfighter = gs_texture_create_from_file(path);
	bfree(path);

	struct gs_sampler_info si = {
		.filter = GS_FILTER_POINT,
		.max_anisotropy = 1,
	};
	ul->nearest_override = gs_samplerstate_create(&si);

	/* ---------------------------------- */
	/* create flag trail data             */

	path = obs_module_file("flag-shader.effect");
	ul->flag_effect = gs_effect_create_from_file(path, NULL);
	bfree(path);

	path = obs_module_file("transflag.webp");
	ul->trans_flag = gs_texture_create_from_file(path);
	bfree(path);

	path = obs_module_file("lesbianflag.webp");
	ul->lesbian_flag = gs_texture_create_from_file(path);
	bfree(path);

	obs_leave_graphics();

	/* ---------------------------------- */
	/* generate star positions            */

	struct vec2 size = {F_CX, F_CY};

#define VARIATION_AMOUNT 0.6f

	for (size_t i = 0; i < NUM_DARK; i++) {
		vec2_rand(&ul->dark_stars[i], true);
		vec2_mul(&ul->dark_stars[i], &ul->dark_stars[i], &size);
		float variation = VARIATION_AMOUNT * rand_float(true);
		ul->dark_variations[i] = 1.0f - variation;
	}

	for (size_t i = 0; i < NUM_MID; i++) {
		vec2_rand(&ul->mid_stars[i], true);
		vec2_mul(&ul->mid_stars[i], &ul->mid_stars[i], &size);
		float variation = VARIATION_AMOUNT * rand_float(true);
		ul->mid_variations[i] = 1.0f - variation;
	}

	for (size_t i = 0; i < NUM_LIGHT; i++) {
		vec2_rand(&ul->light_stars[i], true);
		vec2_mul(&ul->light_stars[i], &ul->light_stars[i], &size);
		float variation = VARIATION_AMOUNT * rand_float(true);
		ul->light_variations[i] = 1.0f - variation;
	}

	/* ---------------------------------- */

	lain_underlay_update(ul, settings);
	return ul;
}

static void lain_underlay_destroy(void *data)
{
	struct lain_underlay *ul = data;

	obs_enter_graphics();
	gs_texture_destroy(ul->star);
	gs_vertexbuffer_destroy(ul->vb);
	gs_texture_destroy(ul->starfighter);
	gs_samplerstate_destroy(ul->nearest_override);
	gs_texture_destroy(ul->trans_flag);
	gs_texture_destroy(ul->lesbian_flag);
	obs_leave_graphics();

	bfree(ul);
}

static inline void move_stars(struct vec2 *stars, float *variations,
			      size_t count, float distance, float max,
			      float scale)
{
	/* moves stars through the field and adds randomized variations to them
	 * each time. moves stars back again when they go offscreen */
	for (size_t i = 0; i < count; i++) {
		stars[i].x += distance;
		if (stars[i].x > (max + scale)) {
			stars[i].x = -scale - rand_float(true) * 8.0f * scale;
			stars[i].y = rand_float(true) * F_CY;
			float variation = VARIATION_AMOUNT * rand_float(true);
			variations[i] = 1.0f - variation;
		}
	}
}

/* clang-format off */
#define DARK_SCALE  15.0f
#define MID_SCALE   27.0f
#define LIGHT_SCALE 40.0f
/* clang-format on */

static void lain_underlay_video_tick(void *data, float seconds)
{
	struct lain_underlay *ul = data;
	if (!obs_source_active(ul->source))
		return;

	/* --------------------------------- */
	/* move three star layers            */

	/* TODO: use frustrum */

	move_stars(ul->dark_stars, ul->dark_variations, NUM_DARK,
		   seconds * F_CX / 8.0f, F_CX, DARK_SCALE + 2.0f);
	move_stars(ul->mid_stars, ul->mid_variations, NUM_MID,
		   seconds * F_CX / 6.0f, F_CX, MID_SCALE + 2.0f);
	move_stars(ul->light_stars, ul->light_variations, NUM_LIGHT,
		   seconds * F_CX / 4.0f, F_CX, LIGHT_SCALE + 2.0f);

	/* --------------------------------- */
	/* do starfighter movement           */

	ul->starfighter_offset_deg += seconds * 180.0f;
	if (ul->starfighter_offset_deg > 360.0f)
		ul->starfighter_offset_deg -= 360.0f;

	ul->starfighter_offset_y = cosf(RAD(ul->starfighter_offset_deg));

	/* --------------------------------- */
	/* do flag pulse movement            */

	ul->flag_pulse_offset_deg += seconds * 180.0f * 5.0f;
	while (ul->flag_pulse_offset_deg > 360.0f)
		ul->flag_pulse_offset_deg -= 360.0f;
}

static void draw_stars(gs_effect_t *effect, gs_texture_t *star,
		       struct vec2 *stars, float *variations, size_t count,
		       gs_eparam_t *image, gs_eparam_t *multiplier,
		       float lightness, float scale)
{
	for (size_t i = 0; i < count; i++) {
		struct vec2 pos = stars[i];
		gs_matrix_push();
		gs_matrix_translate3f(pos.x, pos.y, 0.0f);
		gs_matrix_scale3f(scale, scale, 1.0f);
		gs_effect_set_texture(image, star);
		gs_effect_set_float(multiplier, lightness * variations[i]);
		while (gs_effect_loop(effect, "DrawMultiply"))
			gs_draw(GS_TRISTRIP, 0, 0);
		gs_matrix_pop();
	}
}

/* clang-format off */
#define SHIP_BASE_Y_POS   940.0f
#define SHIP_BOB_RADIUS   20.0f
#define SHIP_BOB_DIAMETER (SHIP_BOB_RADIUS * 2.0f)
/* clang-format on */

static void draw_starfighter(struct lain_underlay *ul, gs_effect_t *effect,
			     gs_eparam_t *image)
{
	/* --------------------------------- */
	/* draw ship itself                  */

	/* I drew the ship facing right so I just reversed the direction in the
	 * code */
	float ship_cx = (float)gs_texture_get_width(ul->starfighter) * 3.0f;
	float ship_cy = (float)gs_texture_get_height(ul->starfighter) * 3.0f;
	float ship_x_pos = 100.0f + ship_cx; /* adding cx because reversed */
	float ship_y_pos =
		SHIP_BASE_Y_POS + ul->starfighter_offset_y * SHIP_BOB_RADIUS;

	gs_matrix_push();
	gs_matrix_translate3f(ship_x_pos, ship_y_pos, 0.0f);
	gs_matrix_scale3f(-ship_cx, ship_cy, 1.0f);
	gs_effect_set_texture(image, ul->starfighter);
	gs_effect_set_next_sampler(image, ul->nearest_override);
	while (gs_effect_loop(effect, "Draw"))
		gs_draw(GS_TRISTRIP, 0, 0);
	gs_matrix_pop();

	/* --------------------------------- */
	/* draw flag trail (single quad)     */

	gs_eparam_t *trans_flag =
		gs_effect_get_param_by_name(ul->flag_effect, "trans_flag");
	gs_eparam_t *lesbian_flag =
		gs_effect_get_param_by_name(ul->flag_effect, "lesbian_flag");
	gs_eparam_t *wave_rot =
		gs_effect_get_param_by_name(ul->flag_effect, "wave_rot");
	gs_eparam_t *pulse_rot =
		gs_effect_get_param_by_name(ul->flag_effect, "pulse_rot");

	gs_matrix_push();
	gs_matrix_translate3f(ship_x_pos, SHIP_BASE_Y_POS - SHIP_BOB_RADIUS,
			      0.0f);
	gs_matrix_scale3f(F_CX - ship_x_pos, ship_cy + SHIP_BOB_DIAMETER, 1.0f);
	gs_effect_set_texture(trans_flag, ul->trans_flag);
	gs_effect_set_texture(lesbian_flag, ul->lesbian_flag);
	gs_effect_set_float(wave_rot, ul->starfighter_offset_deg);
	gs_effect_set_float(pulse_rot, ul->flag_pulse_offset_deg);
	while (gs_effect_loop(ul->flag_effect, "Draw"))
		gs_draw(GS_TRISTRIP, 0, 0);
	gs_matrix_pop();
}

static void lain_underlay_video_render(void *data, gs_effect_t *unused)
{
	struct lain_underlay *ul = data;
	if (!obs_source_active(ul->source))
		return;

	/* this is a custom render source so we use our own effect */
	UNUSED_PARAMETER(unused);

	/* ---------------------------------- */
	/* get default effect for ship/stars  */

	gs_effect_t *def = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t *image = gs_effect_get_param_by_name(def, "image");
	gs_eparam_t *multiplier =
		gs_effect_get_param_by_name(def, "multiplier");

	/* ---------------------------------- */
	/* draw stars                         */

	gs_load_vertexbuffer(ul->vb);
	gs_load_indexbuffer(NULL);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ONE);

	draw_stars(def, ul->star, ul->dark_stars, ul->dark_variations, NUM_DARK,
		   image, multiplier, 0.5f, DARK_SCALE);
	draw_stars(def, ul->star, ul->mid_stars, ul->mid_variations, NUM_MID,
		   image, multiplier, 0.7f, MID_SCALE);
	draw_stars(def, ul->star, ul->light_stars, ul->light_variations,
		   NUM_LIGHT, image, multiplier, 1.0f, LIGHT_SCALE);

	gs_blend_state_pop();

	/* ---------------------------------- */
	/* draw starfighter                   */

	if (ul->show_ship)
		draw_starfighter(ul, def, image);
}

static obs_properties_t *lain_underlay_properties(void *unused)
{
	/* just a property to show the ship */
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_bool(props, "ship", "ship");

	UNUSED_PARAMETER(unused);
	return props;
}

/* hardcoded dimensions */

static uint32_t lain_underlay_width(void *unused)
{
	UNUSED_PARAMETER(unused);
	return CX;
}

static uint32_t lain_underlay_height(void *unused)
{
	UNUSED_PARAMETER(unused);
	return CY;
}

/* source definition */

struct obs_source_info lain_underlay_info = {
	.id = "lain_background",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = lain_underlay_get_name,
	.create = lain_underlay_create,
	.destroy = lain_underlay_destroy,
	.update = lain_underlay_update,
	.video_tick = lain_underlay_video_tick,
	.video_render = lain_underlay_video_render,
	.get_width = lain_underlay_width,
	.get_height = lain_underlay_height,
	.get_properties = lain_underlay_properties,
};

/* ========================================================================= */

bool obs_module_load(void)
{
	obs_register_source(&lain_underlay_info);
	return true;
}
