template<int BlendMode, bool MaskEval_TA, bool textured>
INLINE void PS_GPU::PlotPixel(int32 x, int32 y, uint16_t fore_pix)
{
 y &= 511;	// More Y precision bits than GPU RAM installed in (non-arcade, at least) Playstation hardware.

 if(BlendMode >= 0 && (fore_pix & 0x8000))
 {
  uint16 bg_pix = GPURAM[y][x];	// Don't use bg_pix for mask evaluation, it's modified in blending code paths.
  uint16 pix; // = fore_pix & 0x8000;

/*
 static const int32 tab[4][2] =
 {
  { 2,  2 },
  { 4,  4 },
  { 4, -4 },
  { 4,  1 }
 };
*/
  // Efficient 15bpp pixel math algorithms from blargg
  switch(BlendMode)
  {
   case 0:
	bg_pix |= 0x8000;
	pix = ((fore_pix + bg_pix) - ((fore_pix ^ bg_pix) & 0x0421)) >> 1;
	break;
	  
   case 1:
       {
	bg_pix &= ~0x8000;

	uint32 sum = fore_pix + bg_pix;
	uint32 carry = (sum - ((fore_pix ^ bg_pix) & 0x8421)) & 0x8420;

	pix = (sum - carry) | (carry - (carry >> 5));
       }
       break;

   case 2:
       {
	bg_pix |= 0x8000;
        fore_pix &= ~0x8000;

	uint32 diff = bg_pix - fore_pix + 0x108420;
	uint32 borrow = (diff - ((bg_pix ^ fore_pix) & 0x108420)) & 0x108420;

	pix = (diff - borrow) & (borrow - (borrow >> 5));
       }
       break;

   case 3:
       {
	bg_pix &= ~0x8000;
	fore_pix = ((fore_pix >> 2) & 0x1CE7) | 0x8000;

	uint32 sum = fore_pix + bg_pix;
	uint32 carry = (sum - ((fore_pix ^ bg_pix) & 0x8421)) & 0x8420;

	pix = (sum - carry) | (carry - (carry >> 5));
       }
       break;
  }

  if(!MaskEval_TA || !(GPURAM[y][x] & 0x8000))
   GPURAM[y][x] = (textured ? pix : (pix & 0x7FFF)) | MaskSetOR;
 }
 else
 {
  if(!MaskEval_TA || !(GPURAM[y][x] & 0x8000))
   GPURAM[y][x] = (textured ? fore_pix : (fore_pix & 0x7FFF)) | MaskSetOR;
 }
}

INLINE uint16_t PS_GPU::ModTexel(uint16_t texel, int32 r, int32 g, int32 b, const int32 dither_x, const int32 dither_y)
{
   uint16_t ret = texel & 0x8000;

   ret |= DitherLUT[dither_y][dither_x][(((texel & 0x1F) * r) >> (5 - 1))] << 0;
   ret |= DitherLUT[dither_y][dither_x][(((texel & 0x3E0) * g) >> (10 - 1))] << 5;
   ret |= DitherLUT[dither_y][dither_x][(((texel & 0x7C00) * b) >> (15 - 1))] << 10;

   return(ret);
}

template<uint32 TexMode_TA>
INLINE void PS_GPU::Update_CLUT_Cache(uint16 raw_clut)
{
 if(TexMode_TA < 2)
 {
  const uint32 new_ccvb = ((raw_clut & 0x7FFF) | (TexMode_TA << 16));	// Confirmed upper bit of raw_clut is ignored(at least on SCPH-5501's GPU).

  if(CLUT_Cache_VB != new_ccvb)
  {
   uint16* const gpulp = GPURAM[(raw_clut >> 6) & 0x1FF];
   const uint32 cxo = (raw_clut & 0x3F) << 4;
  const uint32 count = (TexMode_TA ? 256 : 16);

   DrawTimeAvail -= count;

   for(unsigned i = 0; i < count; i++)
   {
    CLUT_Cache[i] = gpulp[(cxo + i) & 0x3FF];
   }

   CLUT_Cache_VB = new_ccvb;
  }
 }
}

#if 0
 TexWindowX_AND = ~(tww << 3);
 TexWindowX_ADD = ((twx & tww) << 3;

 TexWindowY_AND = ~(twh << 3);
 TexWindowY_OR = (twy & twh) << 3;

     uint32 u = (u_arg & TexWindowX_AND)  TexWindowX_OR;
     uint32 v = (v_arg & TexWindowY_AND) | TexWindowY_OR;
     uint32 fbtex_x = TexPageX + (u >> (2 - TexMode_TA));
     uint32 fbtex_y = TexPageY + v;
     uint16 fbw = GPURAM[fbtex_y][fbtex_x & 1023];

     if(TexMode_TA != 2)
     {
      if(TexMode_TA == 0)
       fbw = (fbw >> ((u & 3) * 4)) & 0xF;
      else
       fbw = (fbw >> ((u & 1) * 8)) & 0xFF;
 
      fbw = CLUT_Cache[fbw];
     }
#endif

INLINE void PS_GPU::RecalcTexWindowStuff(void)
{
#if 0
   /* TODO */
   SUCV.TWX_AND = ~(tww << 3);
   SUCV.TWX_ADD = ((twx & tww) << 3) + (TexPageX << (2 - std::min<uint32>(2, TexMode)));

   SUCV.TWY_AND = ~(twh << 3);
   SUCV.TWY_ADD = ((twy & twh) << 3) + TexPageY;
#else
   unsigned x, y;
   const unsigned TexWindowX_AND = ~(tww << 3);
   const unsigned TexWindowX_OR = (twx & tww) << 3;
   const unsigned TexWindowY_AND = ~(twh << 3);
   const unsigned TexWindowY_OR = (twy & twh) << 3;
   // printf("TWX: 0x%02x, TWW: 0x%02x\n", twx, tww);
   // printf("TWY: 0x%02x, TWH: 0x%02x\n", twy, twh);
   for(x = 0; x < 256; x++)
      TexWindowXLUT[x] = (x & TexWindowX_AND) | TexWindowX_OR;
   for(y = 0; y < 256; y++)
      TexWindowYLUT[y] = (y & TexWindowY_AND) | TexWindowY_OR;
   memset(TexWindowXLUT_Pre, TexWindowXLUT[0], sizeof(TexWindowXLUT_Pre));
   memset(TexWindowXLUT_Post, TexWindowXLUT[255], sizeof(TexWindowXLUT_Post));
   memset(TexWindowYLUT_Pre, TexWindowYLUT[0], sizeof(TexWindowYLUT_Pre));
   memset(TexWindowYLUT_Post, TexWindowYLUT[255], sizeof(TexWindowYLUT_Post));
#endif
}

template<uint32_t TexMode_TA>
INLINE uint16_t PS_GPU::GetTexel(const uint32_t clut_offset, int32 u_arg, int32 v_arg)
{
#if 0
   /* TODO */
   uint32 u_ext = ((u_arg & SUCV.TWX_AND) + SUCV.TWX_ADD);
   uint32 fbtex_x = ((u_ext >> (2 - TexMode_TA))) & 1023;
   uint32 fbtex_y = (v_arg & SUCV.TWY_AND) + SUCV.TWY_ADD;
   uint32 gro = fbtex_y * 1024U + fbtex_x;

   decltype(&TexCache[0]) c;

   switch(TexMode_TA)
   {
      case 0: c = &TexCache[((gro >> 2) & 0x3) | ((gro >> 8) & 0xFC)]; break;	// 64x64
      case 1: c = &TexCache[((gro >> 2) & 0x7) | ((gro >> 7) & 0xF8)]; break;	// 64x32 (NOT 32x64!)
      case 2: c = &TexCache[((gro >> 2) & 0x7) | ((gro >> 7) & 0xF8)]; break;	// 32x32
   }

   if(MDFN_UNLIKELY(c->Tag != (gro &~ 0x3)))
   {
      // SCPH-1001 old revision GPU is like(for sprites at least): (20 + 4)
      // SCPH-5501 new revision GPU is like(for sprites at least): (12 + 4)
      //
      // We'll be conservative and just go with 4 for now, until we can run some tests with triangles too.
      //
      DrawTimeAvail -= 4;
      c->Data[0] = (&GPURAM[0][0])[gro &~ 0x3];
      c->Data[1] = (&GPURAM[0][1])[gro &~ 0x3];
      c->Data[2] = (&GPURAM[0][2])[gro &~ 0x3];
      c->Data[3] = (&GPURAM[0][3])[gro &~ 0x3];
      c->Tag = (gro &~ 0x3);
   }

   uint16 fbw = c->Data[gro & 0x3];

   if(TexMode_TA != 2)
   {
      if(TexMode_TA == 0)
         fbw = (fbw >> ((u_ext & 3) * 4)) & 0xF;
      else
         fbw = (fbw >> ((u_ext & 1) * 8)) & 0xFF;

      fbw = CLUT_Cache[fbw];
   }
#else
   uint16_t fbw;
   uint32_t u_ext, v, fbtex_x, fbtex_y;

   u_ext = TexWindowXLUT[u_arg];
   v = TexWindowYLUT[v_arg];
   fbtex_x = TexPageX + (u_ext >> (2 - TexMode_TA));
   fbtex_y = TexPageY + v;
   fbw = GPURAM[fbtex_y][fbtex_x & 1023];

   if(TexMode_TA != 2)
   {
      if(TexMode_TA == 0)
         fbw = (fbw >> ((u_ext & 3) * 4)) & 0xF;
      else
         fbw = (fbw >> ((u_ext & 1) * 8)) & 0xFF;

      fbw = GPURAM[(clut_offset >> 10) & 511][(clut_offset + fbw) & 1023];
   }
#endif

   return(fbw);
}

static INLINE bool LineSkipTest(PS_GPU* g, unsigned y)
{
   //DisplayFB_XStart >= OffsX && DisplayFB_YStart >= OffsY &&
   // ((y & 1) == (DisplayFB_CurLineYReadout & 1))

   if((g->DisplayMode & 0x24) != 0x24)
      return false;

   if(!g->dfe && ((y & 1) == ((g->DisplayFB_YStart + g->field_ram_readout) & 1))/* && !DisplayOff*/) //&& (y >> 1) >= DisplayFB_YStart && (y >> 1) < (DisplayFB_YStart + (VertEnd - VertStart)))
      return true;

   return false;
}

//
// Command table generation macros follow:
//

//#define BM_HELPER(fg) { fg(0), fg(1), fg(2), fg(3) }

#define POLY_HELPER_SUB(bm, cv, tm, mam)	\
	 G_Command_DrawPolygon<3 + ((cv & 0x8) >> 3), ((cv & 0x10) >> 4), ((cv & 0x4) >> 2), ((cv & 0x2) >> 1) ? bm : -1, ((cv & 1) ^ 1) & ((cv & 0x4) >> 2), tm, mam >

#define POLY_HELPER_FG(bm, cv)						\
	 {								\
		POLY_HELPER_SUB(bm, cv, ((cv & 0x4) ? 0 : 0), 0),	\
		POLY_HELPER_SUB(bm, cv, ((cv & 0x4) ? 1 : 0), 0),	\
		POLY_HELPER_SUB(bm, cv, ((cv & 0x4) ? 2 : 0), 0),	\
		POLY_HELPER_SUB(bm, cv, ((cv & 0x4) ? 2 : 0), 0),	\
		POLY_HELPER_SUB(bm, cv, ((cv & 0x4) ? 0 : 0), 1),	\
		POLY_HELPER_SUB(bm, cv, ((cv & 0x4) ? 1 : 0), 1),	\
		POLY_HELPER_SUB(bm, cv, ((cv & 0x4) ? 2 : 0), 1),	\
		POLY_HELPER_SUB(bm, cv, ((cv & 0x4) ? 2 : 0), 1),	\
	 }

#define POLY_HELPER(cv)														\
	{ 															\
	 { POLY_HELPER_FG(0, cv), POLY_HELPER_FG(1, cv), POLY_HELPER_FG(2, cv), POLY_HELPER_FG(3, cv) },			\
	 1 + (3 /*+ ((cv & 0x8) >> 3)*/) * ( 1 + ((cv & 0x4) >> 2) + ((cv & 0x10) >> 4) ) - ((cv & 0x10) >> 4),			\
	 1,															\
 	 false															\
	}

//
//

#define SPR_HELPER_SUB(bm, cv, tm, mam) G_Command_DrawSprite<(cv >> 3) & 0x3,	((cv & 0x4) >> 2), ((cv & 0x2) >> 1) ? bm : -1, ((cv & 1) ^ 1) & ((cv & 0x4) >> 2), tm, mam>

#define SPR_HELPER_FG(bm, cv)						\
	 {								\
		SPR_HELPER_SUB(bm, cv, ((cv & 0x4) ? 0 : 0), 0),	\
		SPR_HELPER_SUB(bm, cv, ((cv & 0x4) ? 1 : 0), 0),	\
		SPR_HELPER_SUB(bm, cv, ((cv & 0x4) ? 2 : 0), 0),	\
		SPR_HELPER_SUB(bm, cv, ((cv & 0x4) ? 2 : 0), 0),	\
		SPR_HELPER_SUB(bm, cv, ((cv & 0x4) ? 0 : 0), 1),	\
		SPR_HELPER_SUB(bm, cv, ((cv & 0x4) ? 1 : 0), 1),	\
		SPR_HELPER_SUB(bm, cv, ((cv & 0x4) ? 2 : 0), 1),	\
		SPR_HELPER_SUB(bm, cv, ((cv & 0x4) ? 2 : 0), 1),	\
	 }


#define SPR_HELPER(cv)												\
	{													\
	 { SPR_HELPER_FG(0, cv), SPR_HELPER_FG(1, cv), SPR_HELPER_FG(2, cv), SPR_HELPER_FG(3, cv) },		\
	 2 + ((cv & 0x4) >> 2) + ((cv & 0x18) ? 0 : 1),								\
	 2 | ((cv & 0x4) >> 2) | ((cv & 0x18) ? 0 : 1),		/* |, not +, for this */			\
	 false													\
	}

//
//

#define LINE_HELPER_SUB(bm, cv, mam) G_Command_DrawLine<((cv & 0x08) >> 3), ((cv & 0x10) >> 4), ((cv & 0x2) >> 1) ? bm : -1, mam>

#define LINE_HELPER_FG(bm, cv)											\
	 {													\
		LINE_HELPER_SUB(bm, cv, 0),									\
		LINE_HELPER_SUB(bm, cv, 0),									\
		LINE_HELPER_SUB(bm, cv, 0),									\
		LINE_HELPER_SUB(bm, cv, 0),									\
		LINE_HELPER_SUB(bm, cv, 1),									\
		LINE_HELPER_SUB(bm, cv, 1),									\
		LINE_HELPER_SUB(bm, cv, 1),									\
		LINE_HELPER_SUB(bm, cv, 1)									\
	 }

#define LINE_HELPER(cv)												\
	{ 													\
	 { LINE_HELPER_FG(0, cv), LINE_HELPER_FG(1, cv), LINE_HELPER_FG(2, cv), LINE_HELPER_FG(3, cv) },	\
	 3 + ((cv & 0x10) >> 4),										\
	 1,													\
	 false													\
	}

//
//

#define OTHER_HELPER_FG(bm, arg_ptr) { arg_ptr, arg_ptr, arg_ptr, arg_ptr, arg_ptr, arg_ptr, arg_ptr, arg_ptr }
#define OTHER_HELPER(arg_cs, arg_fbcs, arg_ss, arg_ptr) { { OTHER_HELPER_FG(0, arg_ptr), OTHER_HELPER_FG(1, arg_ptr), OTHER_HELPER_FG(2, arg_ptr), OTHER_HELPER_FG(3, arg_ptr) }, arg_cs, arg_fbcs, arg_ss }
#define OTHER_HELPER_X2(arg_cs, arg_fbcs, arg_ss, arg_ptr)	OTHER_HELPER(arg_cs, arg_fbcs, arg_ss, arg_ptr), OTHER_HELPER(arg_cs, arg_fbcs, arg_ss, arg_ptr)
#define OTHER_HELPER_X4(arg_cs, arg_fbcs, arg_ss, arg_ptr)	OTHER_HELPER_X2(arg_cs, arg_fbcs, arg_ss, arg_ptr), OTHER_HELPER_X2(arg_cs, arg_fbcs, arg_ss, arg_ptr)
#define OTHER_HELPER_X8(arg_cs, arg_fbcs, arg_ss, arg_ptr)	OTHER_HELPER_X4(arg_cs, arg_fbcs, arg_ss, arg_ptr), OTHER_HELPER_X4(arg_cs, arg_fbcs, arg_ss, arg_ptr)
#define OTHER_HELPER_X16(arg_cs, arg_fbcs, arg_ss, arg_ptr)	OTHER_HELPER_X8(arg_cs, arg_fbcs, arg_ss, arg_ptr), OTHER_HELPER_X8(arg_cs, arg_fbcs, arg_ss, arg_ptr)
#define OTHER_HELPER_X32(arg_cs, arg_fbcs, arg_ss, arg_ptr)	OTHER_HELPER_X16(arg_cs, arg_fbcs, arg_ss, arg_ptr), OTHER_HELPER_X16(arg_cs, arg_fbcs, arg_ss, arg_ptr)

#define NULLCMD_FG(bm) { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL } 
#define NULLCMD() { { NULLCMD_FG(0), NULLCMD_FG(1), NULLCMD_FG(2), NULLCMD_FG(3) }, 1, 1, true }
