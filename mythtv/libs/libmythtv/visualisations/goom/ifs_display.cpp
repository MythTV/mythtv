#include <array>

#include "ifs.h"
#include "goomconfig.h"

#ifdef MMX
#include "mmx.h"
#endif

#include "goom_tools.h"

/* NOLINTNEXTLINE(readability-non-const-parameter) */
void ifs_update (guint32 * data, const guint32 * back, int width, int height,
						int increment)
{
	static int s_couleur = 0xc0c0c0c0;
	static std::array<int,4> s_v { 2, 4, 3, 2 };
	static std::array<int,4> s_col { 2, 4, 3, 2 };

        enum MODE : std::uint8_t {
            MOD_MER    = 0,
            MOD_FEU    = 1,
            MOD_MERVER = 2
        };
	static MODE s_mode = MOD_MERVER;
	static int s_justChanged = 0;
	static int s_cycle = 0;
	int     cycle10 = 0;
	int     couleursl = s_couleur;

	s_cycle++;
	if (s_cycle >= 80)
		s_cycle = 0;

	if (s_cycle < 40)
		cycle10 = s_cycle / 10;
	else
		cycle10 = 7 - (s_cycle / 10);

	{
		auto *tmp = (unsigned char *) &couleursl;

		for (int i = 0; i < 4; i++) {
			*tmp = (*tmp) >> cycle10;
			tmp++;
		}
	}

	int nbpt = 0;
	IFSPoint *points = draw_ifs (&nbpt);
	nbpt--;

#ifdef MMX
	movd_m2r (couleursl, mm1);
	punpckldq_r2r (mm1, mm1);
	for (int i = 0; i < nbpt; i += increment) {
		int     x = points[i].x;
		int     y = points[i].y;

		if ((x < width) && (y < height) && (x > 0) && (y > 0)) {
			int     pos = x + (y * width);
			movd_m2r (back[pos], mm0);
			paddusb_r2r (mm1, mm0);
			movd_r2m (mm0, data[pos]);
		}
	}
	emms();/*__asm__ __volatile__ ("emms");*/
#else
	for (int i = 0; i < nbpt; i += increment) {
		int     x = points[i].x & 0x7fffffff;
		int     y = points[i].y & 0x7fffffff;

		if ((x < width) && (y < height)) {
			int     pos = x + (y * width);
			int     tra = 0;
			auto *bra = (unsigned char *) &back[pos];
			auto *dra = (unsigned char *) &data[pos];
			auto *cra = (unsigned char *) &couleursl;

			for (int j = 0; j < 4; j++) {
				tra = *cra;
				tra += *bra;
				if (tra > 255)
					tra = 255;
				*dra = tra;
				++dra;
				++cra;
				++bra;
			}
		}
	}
#endif /*MMX*/
		s_justChanged--;

	s_col[ALPHA] = s_couleur >> (ALPHA * 8) & 0xff;
	s_col[BLEU]  = s_couleur >> (BLEU * 8)  & 0xff;
	s_col[VERT]  = s_couleur >> (VERT * 8)  & 0xff;
	s_col[ROUGE] = s_couleur >> (ROUGE * 8) & 0xff;

	if (s_mode == MOD_MER) {
		s_col[BLEU] += s_v[BLEU];
		if (s_col[BLEU] > 255) {
			s_col[BLEU] = 255;
			s_v[BLEU] = -(RAND() % 4) - 1;
		}
		if (s_col[BLEU] < 32) {
			s_col[BLEU] = 32;
			s_v[BLEU] = (RAND() % 4) + 1;
		}

		s_col[VERT] += s_v[VERT];
		if (s_col[VERT] > 200) {
			s_col[VERT] = 200;
			s_v[VERT] = -(RAND() % 3) - 2;
		}
		if (s_col[VERT] > s_col[BLEU]) {
			s_col[VERT] = s_col[BLEU];
			s_v[VERT] = s_v[BLEU];
		}
		if (s_col[VERT] < 32) {
			s_col[VERT] = 32;
			s_v[VERT] = (RAND() % 3) + 2;
		}

		s_col[ROUGE] += s_v[ROUGE];
		if (s_col[ROUGE] > 64) {
			s_col[ROUGE] = 64;
			s_v[ROUGE] = -(RAND () % 4) - 1;
		}
		if (s_col[ROUGE] < 0) {
			s_col[ROUGE] = 0;
			s_v[ROUGE] = (RAND () % 4) + 1;
		}

		s_col[ALPHA] += s_v[ALPHA];
		if (s_col[ALPHA] > 0) {
			s_col[ALPHA] = 0;
			s_v[ALPHA] = -(RAND () % 4) - 1;
		}
		if (s_col[ALPHA] < 0) {
			s_col[ALPHA] = 0;
			s_v[ALPHA] = (RAND () % 4) + 1;
		}

		if (((s_col[VERT] > 32) && (s_col[ROUGE] < s_col[VERT] + 40)
				 && (s_col[VERT] < s_col[ROUGE] + 20) && (s_col[BLEU] < 64)
				 && (RAND () % 20 == 0)) && (s_justChanged < 0)) {
			s_mode = (RAND () % 3) ? MOD_FEU : MOD_MERVER;
			s_justChanged = 250;
		}
	}
	else if (s_mode == MOD_MERVER) {
		s_col[BLEU] += s_v[BLEU];
		if (s_col[BLEU] > 128) {
			s_col[BLEU] = 128;
			s_v[BLEU] = -(RAND () % 4) - 1;
		}
		if (s_col[BLEU] < 16) {
			s_col[BLEU] = 16;
			s_v[BLEU] = (RAND () % 4) + 1;
		}

		s_col[VERT] += s_v[VERT];
		if (s_col[VERT] > 200) {
			s_col[VERT] = 200;
			s_v[VERT] = -(RAND () % 3) - 2;
		}
		if (s_col[VERT] > s_col[ALPHA]) {
			s_col[VERT] = s_col[ALPHA];
			s_v[VERT] = s_v[ALPHA];
		}
		if (s_col[VERT] < 32) {
			s_col[VERT] = 32;
			s_v[VERT] = (RAND () % 3) + 2;
		}

		s_col[ROUGE] += s_v[ROUGE];
		if (s_col[ROUGE] > 128) {
			s_col[ROUGE] = 128;
			s_v[ROUGE] = -(RAND () % 4) - 1;
		}
		if (s_col[ROUGE] < 0) {
			s_col[ROUGE] = 0;
			s_v[ROUGE] = (RAND () % 4) + 1;
		}

		s_col[ALPHA] += s_v[ALPHA];
		if (s_col[ALPHA] > 255) {
			s_col[ALPHA] = 255;
			s_v[ALPHA] = -(RAND () % 4) - 1;
		}
		if (s_col[ALPHA] < 0) {
			s_col[ALPHA] = 0;
			s_v[ALPHA] = (RAND () % 4) + 1;
		}

		if (((s_col[VERT] > 32) && (s_col[ROUGE] < s_col[VERT] + 40)
				 && (s_col[VERT] < s_col[ROUGE] + 20) && (s_col[BLEU] < 64)
				 && (RAND () % 20 == 0)) && (s_justChanged < 0)) {
			s_mode = (RAND () % 3) ? MOD_FEU : MOD_MER;
			s_justChanged = 250;
		}
	}
	else if (s_mode == MOD_FEU) {

		s_col[BLEU] += s_v[BLEU];
		if (s_col[BLEU] > 64) {
			s_col[BLEU] = 64;
			s_v[BLEU] = -(RAND () % 4) - 1;
		}
		if (s_col[BLEU] < 0) {
			s_col[BLEU] = 0;
			s_v[BLEU] = (RAND () % 4) + 1;
		}

		s_col[VERT] += s_v[VERT];
		if (s_col[VERT] > 200) {
			s_col[VERT] = 200;
			s_v[VERT] = -(RAND () % 3) - 2;
		}
		if (s_col[VERT] > s_col[ROUGE] + 20) {
			s_col[VERT] = s_col[ROUGE] + 20;
			s_v[VERT] = -(RAND () % 3) - 2;
			s_v[ROUGE] = (RAND () % 4) + 1;
			s_v[BLEU] = (RAND () % 4) + 1;
		}
		if (s_col[VERT] < 0) {
			s_col[VERT] = 0;
			s_v[VERT] = (RAND () % 3) + 2;
		}

		s_col[ROUGE] += s_v[ROUGE];
		if (s_col[ROUGE] > 255) {
			s_col[ROUGE] = 255;
			s_v[ROUGE] = -(RAND () % 4) - 1;
		}
		if (s_col[ROUGE] > s_col[VERT] + 40) {
			s_col[ROUGE] = s_col[VERT] + 40;
			s_v[ROUGE] = -(RAND () % 4) - 1;
		}
		if (s_col[ROUGE] < 0) {
			s_col[ROUGE] = 0;
			s_v[ROUGE] = (RAND () % 4) + 1;
		}

		s_col[ALPHA] += s_v[ALPHA];
		if (s_col[ALPHA] > 0) {
			s_col[ALPHA] = 0;
			s_v[ALPHA] = -(RAND () % 4) - 1;
		}
		if (s_col[ALPHA] < 0) {
			s_col[ALPHA] = 0;
			s_v[ALPHA] = (RAND () % 4) + 1;
		}

		if (((s_col[ROUGE] < 64) && (s_col[VERT] > 32) && (s_col[VERT] < s_col[BLEU])
				 && (s_col[BLEU] > 32)
				 && (RAND () % 20 == 0)) && (s_justChanged < 0)) {
			s_mode = (RAND () % 2) ? MOD_MER : MOD_MERVER;
			s_justChanged = 250;
		}
	}

	s_couleur = (s_col[ALPHA] << (ALPHA * 8))
		| (s_col[BLEU] << (BLEU * 8))
		| (s_col[VERT] << (VERT * 8))
		| (s_col[ROUGE] << (ROUGE * 8));
}
