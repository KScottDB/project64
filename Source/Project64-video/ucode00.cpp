/***************************************************************************
*                                                                          *
* Project64-video - A Nintendo 64 gfx plugin.                              *
* http://www.pj64-emu.com/                                                 *
* Copyright (C) 2017 Project64. All rights reserved.                       *
* Copyright (C) 2003-2009  Sergey 'Gonetz' Lipski                          *
* Copyright (C) 2002 Dave2001                                              *
*                                                                          *
* License:                                                                 *
* GNU/GPLv2 http://www.gnu.org/licenses/gpl-2.0.html                       *
* version 2 of the License, or (at your option) any later version.         *
*                                                                          *
****************************************************************************/
#include <Project64-video/rdp.h>
#include <Project64-video/Gfx_1.3.h>
#include <Project64-video/trace.h>
#include <Project64-video/ucode.h>
#include <math.h>
#include "3dmath.h"
#include "Util.h"

void rsp_vertex(int v0, int n)
{
    uint32_t addr = segoffset(rdp.cmd1) & 0x00FFFFFF;
    int i;
    float x, y, z;

    rdp.v0 = v0; // Current vertex
    rdp.vn = n;  // Number to copy

    // This is special, not handled in update(), but here
    // * Matrix Pre-multiplication idea by Gonetz (Gonetz@ngs.ru)
    if (rdp.update & UPDATE_MULT_MAT)
    {
        rdp.update ^= UPDATE_MULT_MAT;
        MulMatrices(rdp.model, rdp.proj, rdp.combined);
    }
    // *

    // This is special, not handled in update()
    if (rdp.update & UPDATE_LIGHTS)
    {
        rdp.update ^= UPDATE_LIGHTS;

        // Calculate light vectors
        for (uint32_t l = 0; l < rdp.num_lights; l++)
        {
            InverseTransformVector(&rdp.light[l].dir_x, rdp.light_vector[l], rdp.model);
            NormalizeVector(rdp.light_vector[l]);
        }
    }

    WriteTrace(TraceRDP, TraceDebug, "rsp:vertex v0:%d, n:%d, from: %08lx", v0, n, addr);

    for (i = 0; i < (n << 4); i += 16)
    {
        gfxVERTEX &v = rdp.vtx(v0 + (i >> 4));
        x = (float)((short*)gfx.RDRAM)[(((addr + i) >> 1) + 0) ^ 1];
        y = (float)((short*)gfx.RDRAM)[(((addr + i) >> 1) + 1) ^ 1];
        z = (float)((short*)gfx.RDRAM)[(((addr + i) >> 1) + 2) ^ 1];
        v.flags = ((uint16_t*)gfx.RDRAM)[(((addr + i) >> 1) + 3) ^ 1];
        v.ou = (float)((short*)gfx.RDRAM)[(((addr + i) >> 1) + 4) ^ 1];
        v.ov = (float)((short*)gfx.RDRAM)[(((addr + i) >> 1) + 5) ^ 1];
        v.uv_scaled = 0;
        v.a = ((uint8_t*)gfx.RDRAM)[(addr + i + 15) ^ 3];

        v.x = x*rdp.combined[0][0] + y*rdp.combined[1][0] + z*rdp.combined[2][0] + rdp.combined[3][0];
        v.y = x*rdp.combined[0][1] + y*rdp.combined[1][1] + z*rdp.combined[2][1] + rdp.combined[3][1];
        v.z = x*rdp.combined[0][2] + y*rdp.combined[1][2] + z*rdp.combined[2][2] + rdp.combined[3][2];
        v.w = x*rdp.combined[0][3] + y*rdp.combined[1][3] + z*rdp.combined[2][3] + rdp.combined[3][3];

        if (fabs(v.w) < 0.001) v.w = 0.001f;
        v.oow = 1.0f / v.w;
        v.x_w = v.x * v.oow;
        v.y_w = v.y * v.oow;
        v.z_w = v.z * v.oow;
        CalculateFog(v);

        v.uv_calculated = 0xFFFFFFFF;
        v.screen_translated = 0;
        v.shade_mod = 0;

        v.scr_off = 0;
        if (v.x < -v.w) v.scr_off |= 1;
        if (v.x > v.w) v.scr_off |= 2;
        if (v.y < -v.w) v.scr_off |= 4;
        if (v.y > v.w) v.scr_off |= 8;
        if (v.w < 0.1f) v.scr_off |= 16;
        //    if (v.z_w > 1.0f) v.scr_off |= 32;

        if (rdp.geom_mode & 0x00020000)
        {
            v.vec[0] = ((char*)gfx.RDRAM)[(addr + i + 12) ^ 3];
            v.vec[1] = ((char*)gfx.RDRAM)[(addr + i + 13) ^ 3];
            v.vec[2] = ((char*)gfx.RDRAM)[(addr + i + 14) ^ 3];
            if (rdp.geom_mode & 0x40000)
            {
                if (rdp.geom_mode & 0x80000)
                    calc_linear(v);
                else
                    calc_sphere(v);
            }
            NormalizeVector(v.vec);

            calc_light(v);
        }
        else
        {
            v.r = ((uint8_t*)gfx.RDRAM)[(addr + i + 12) ^ 3];
            v.g = ((uint8_t*)gfx.RDRAM)[(addr + i + 13) ^ 3];
            v.b = ((uint8_t*)gfx.RDRAM)[(addr + i + 14) ^ 3];
        }
        WriteTrace(TraceRDP, TraceVerbose, "v%d - x: %f, y: %f, z: %f, w: %f, u: %f, v: %f, f: %f, z_w: %f, r=%d, g=%d, b=%d, a=%d", i >> 4, v.x, v.y, v.z, v.w, v.ou*rdp.tiles(rdp.cur_tile).s_scale, v.ov*rdp.tiles(rdp.cur_tile).t_scale, v.f, v.z_w, v.r, v.g, v.b, v.a);
    }
}

void rsp_tri1(gfxVERTEX **v, uint16_t linew = 0)
{
    if (cull_tri(v))
        rdp.tri_n++;
    else
    {
        update();
        draw_tri(v, linew);
        rdp.tri_n++;
    }
}

void rsp_tri2(gfxVERTEX **v)
{
    int updated = 0;

    if (cull_tri(v))
        rdp.tri_n++;
    else
    {
        updated = 1;
        update();

        draw_tri(v);
        rdp.tri_n++;
    }

    if (cull_tri(v + 3))
        rdp.tri_n++;
    else
    {
        if (!updated)
            update();

        draw_tri(v + 3);
        rdp.tri_n++;
    }
}

//
// uc0:vertex - loads vertices
//
void uc0_vertex()
{
    int v0 = (rdp.cmd0 >> 16) & 0xF;      // Current vertex
    int n = ((rdp.cmd0 >> 20) & 0xF) + 1; // Number of vertices to copy
    rsp_vertex(v0, n);
}

// ** Definitions **

void modelview_load(float m[4][4])
{
    memcpy(rdp.model, m, 64);  // 4*4*4(float)

    rdp.update |= UPDATE_MULT_MAT | UPDATE_LIGHTS;
}

void modelview_mul(float m[4][4])
{
    DECLAREALIGN16VAR(m_src[4][4]);
    memcpy(m_src, rdp.model, 64);
    MulMatrices(m, m_src, rdp.model);
    rdp.update |= UPDATE_MULT_MAT | UPDATE_LIGHTS;
}

void modelview_push()
{
    if (rdp.model_i == rdp.model_stack_size)
    {
        WriteTrace(TraceRDP, TraceWarning, "** Model matrix stack overflow ** too many pushes");
        return;
    }

    memcpy(rdp.model_stack[rdp.model_i], rdp.model, 64);
    rdp.model_i++;
}

void modelview_pop(int num = 1)
{
    if (rdp.model_i > num - 1)
    {
        rdp.model_i -= num;
    }
    else
    {
        WriteTrace(TraceRDP, TraceWarning, "** Model matrix stack error ** too many pops");
        return;
    }
    memcpy(rdp.model, rdp.model_stack[rdp.model_i], 64);
    rdp.update |= UPDATE_MULT_MAT | UPDATE_LIGHTS;
}

void modelview_load_push(float m[4][4])
{
    modelview_push();
    modelview_load(m);
}

void modelview_mul_push(float m[4][4])
{
    modelview_push();
    modelview_mul(m);
}

void projection_load(float m[4][4])
{
    memcpy(rdp.proj, m, 64); // 4*4*4(float)

    rdp.update |= UPDATE_MULT_MAT;
}

void projection_mul(float m[4][4])
{
    DECLAREALIGN16VAR(m_src[4][4]);
    memcpy(m_src, rdp.proj, 64);
    MulMatrices(m, m_src, rdp.proj);
    rdp.update |= UPDATE_MULT_MAT;
}

void load_matrix(float m[4][4], uint32_t addr)
{
    WriteTrace(TraceRDP, TraceDebug, "matrix - addr: %08lx", addr);
    int x, y;  // matrix index
    addr >>= 1;
    uint16_t * src = (uint16_t*)gfx.RDRAM;
    for (x = 0; x < 16; x += 4) { // Adding 4 instead of one, just to remove mult. later
        for (y = 0; y < 4; y++) {
            m[x >> 2][y] = (float)(
                (((int32_t)src[(addr + x + y) ^ 1]) << 16) |
                src[(addr + x + y + 16) ^ 1]
                ) / 65536.0f;
        }
    }
}

//
// uc0:matrix - performs matrix operations
//
void uc0_matrix()
{
    WriteTrace(TraceRDP, TraceDebug, "uc0:matrix ");

    // Use segment offset to get the address
    uint32_t addr = segoffset(rdp.cmd1) & 0x00FFFFFF;
    uint8_t command = (uint8_t)((rdp.cmd0 >> 16) & 0xFF);

    DECLAREALIGN16VAR(m[4][4]);
    load_matrix(m, addr);

    switch (command)
    {
    case 0: // modelview mul nopush
        WriteTrace(TraceRDP, TraceDebug, "modelview mul");
        modelview_mul(m);
        break;

    case 1: // projection mul nopush
    case 5: // projection mul push, can't push projection
        WriteTrace(TraceRDP, TraceDebug, "projection mul");
        projection_mul(m);
        break;

    case 2: // modelview load nopush
        WriteTrace(TraceRDP, TraceDebug, "modelview load");
        modelview_load(m);
        break;

    case 3: // projection load nopush
    case 7: // projection load push, can't push projection
        WriteTrace(TraceRDP, TraceDebug, "projection load");
        projection_load(m);

        break;

    case 4: // modelview mul push
        WriteTrace(TraceRDP, TraceDebug, "modelview mul push");
        modelview_mul_push(m);
        break;

    case 6: // modelview load push
        WriteTrace(TraceRDP, TraceDebug, "modelview load push");
        modelview_load_push(m);
        break;

    default:
        WriteTrace(TraceRDP, TraceWarning, "Unknown matrix command, %02lx", command);
    }

    WriteTrace(TraceRDP, TraceVerbose, "{%f,%f,%f,%f}", m[0][0], m[0][1], m[0][2], m[0][3]);
    WriteTrace(TraceRDP, TraceVerbose, "{%f,%f,%f,%f}", m[1][0], m[1][1], m[1][2], m[1][3]);
    WriteTrace(TraceRDP, TraceVerbose, "{%f,%f,%f,%f}", m[2][0], m[2][1], m[2][2], m[2][3]);
    WriteTrace(TraceRDP, TraceVerbose, "{%f,%f,%f,%f}", m[3][0], m[3][1], m[3][2], m[3][3]);
    WriteTrace(TraceRDP, TraceVerbose, "\nmodel\n{%f,%f,%f,%f}", rdp.model[0][0], rdp.model[0][1], rdp.model[0][2], rdp.model[0][3]);
    WriteTrace(TraceRDP, TraceVerbose, "{%f,%f,%f,%f}", rdp.model[1][0], rdp.model[1][1], rdp.model[1][2], rdp.model[1][3]);
    WriteTrace(TraceRDP, TraceVerbose, "{%f,%f,%f,%f}", rdp.model[2][0], rdp.model[2][1], rdp.model[2][2], rdp.model[2][3]);
    WriteTrace(TraceRDP, TraceVerbose, "{%f,%f,%f,%f}", rdp.model[3][0], rdp.model[3][1], rdp.model[3][2], rdp.model[3][3]);
    WriteTrace(TraceRDP, TraceVerbose, "\nproj\n{%f,%f,%f,%f}", rdp.proj[0][0], rdp.proj[0][1], rdp.proj[0][2], rdp.proj[0][3]);
    WriteTrace(TraceRDP, TraceVerbose, "{%f,%f,%f,%f}", rdp.proj[1][0], rdp.proj[1][1], rdp.proj[1][2], rdp.proj[1][3]);
    WriteTrace(TraceRDP, TraceVerbose, "{%f,%f,%f,%f}", rdp.proj[2][0], rdp.proj[2][1], rdp.proj[2][2], rdp.proj[2][3]);
    WriteTrace(TraceRDP, TraceVerbose, "{%f,%f,%f,%f}", rdp.proj[3][0], rdp.proj[3][1], rdp.proj[3][2], rdp.proj[3][3]);
}

//
// uc0:movemem - loads a structure with data
//
void uc0_movemem()
{
    WriteTrace(TraceRDP, TraceDebug, "uc0:movemem ");

    uint32_t i, a;

    // Check the command
    switch ((rdp.cmd0 >> 16) & 0xFF)
    {
    case 0x80:
    {
        a = (segoffset(rdp.cmd1) & 0xFFFFFF) >> 1;

        short scale_x = ((short*)gfx.RDRAM)[(a + 0) ^ 1] / 4;
        short scale_y = ((short*)gfx.RDRAM)[(a + 1) ^ 1] / 4;
        short scale_z = ((short*)gfx.RDRAM)[(a + 2) ^ 1];
        short trans_x = ((short*)gfx.RDRAM)[(a + 4) ^ 1] / 4;
        short trans_y = ((short*)gfx.RDRAM)[(a + 5) ^ 1] / 4;
        short trans_z = ((short*)gfx.RDRAM)[(a + 6) ^ 1];
        if (g_settings->correct_viewport())
        {
            scale_x = (short)abs(scale_x);
            scale_y = (short)abs(scale_y);
        }
        rdp.view_scale[0] = scale_x * rdp.scale_x;
        rdp.view_scale[1] = -scale_y * rdp.scale_y;
        rdp.view_scale[2] = 32.0f * scale_z;
        rdp.view_trans[0] = trans_x * rdp.scale_x;
        rdp.view_trans[1] = trans_y * rdp.scale_y;
        rdp.view_trans[2] = 32.0f * trans_z;

        // there are other values than x and y, but I don't know what they do

        rdp.update |= UPDATE_VIEWPORT;

        WriteTrace(TraceRDP, TraceDebug, "viewport scale(%d, %d, %d), trans(%d, %d, %d), from:%08lx", scale_x, scale_y, scale_z,
            trans_x, trans_y, trans_z, rdp.cmd1);
    }
    break;

    case 0x82:
    {
        a = segoffset(rdp.cmd1) & 0x00ffffff;
        char dir_x = ((char*)gfx.RDRAM)[(a + 8) ^ 3];
        rdp.lookat[1][0] = (float)(dir_x) / 127.0f;
        char dir_y = ((char*)gfx.RDRAM)[(a + 9) ^ 3];
        rdp.lookat[1][1] = (float)(dir_y) / 127.0f;
        char dir_z = ((char*)gfx.RDRAM)[(a + 10) ^ 3];
        rdp.lookat[1][2] = (float)(dir_z) / 127.0f;
        if (!dir_x && !dir_y)
            rdp.use_lookat = FALSE;
        else
            rdp.use_lookat = TRUE;
        WriteTrace(TraceRDP, TraceDebug, "lookat_y (%f, %f, %f)", rdp.lookat[1][0], rdp.lookat[1][1], rdp.lookat[1][2]);
    }
    break;

    case 0x84:
        a = segoffset(rdp.cmd1) & 0x00ffffff;
        rdp.lookat[0][0] = (float)(((char*)gfx.RDRAM)[(a + 8) ^ 3]) / 127.0f;
        rdp.lookat[0][1] = (float)(((char*)gfx.RDRAM)[(a + 9) ^ 3]) / 127.0f;
        rdp.lookat[0][2] = (float)(((char*)gfx.RDRAM)[(a + 10) ^ 3]) / 127.0f;
        rdp.use_lookat = TRUE;
        WriteTrace(TraceRDP, TraceDebug, "lookat_x (%f, %f, %f)", rdp.lookat[1][0], rdp.lookat[1][1], rdp.lookat[1][2]);
        break;

    case 0x86:
    case 0x88:
    case 0x8a:
    case 0x8c:
    case 0x8e:
    case 0x90:
    case 0x92:
    case 0x94:
        // Get the light #
        i = (((rdp.cmd0 >> 16) & 0xff) - 0x86) >> 1;
        a = segoffset(rdp.cmd1) & 0x00ffffff;

        // Get the data
        rdp.light[i].r = (float)(((uint8_t*)gfx.RDRAM)[(a + 0) ^ 3]) / 255.0f;
        rdp.light[i].g = (float)(((uint8_t*)gfx.RDRAM)[(a + 1) ^ 3]) / 255.0f;
        rdp.light[i].b = (float)(((uint8_t*)gfx.RDRAM)[(a + 2) ^ 3]) / 255.0f;
        rdp.light[i].a = 1.0f;
        // ** Thanks to Icepir8 for pointing this out **
        // Lighting must be signed byte instead of byte
        rdp.light[i].dir_x = (float)(((char*)gfx.RDRAM)[(a + 8) ^ 3]) / 127.0f;
        rdp.light[i].dir_y = (float)(((char*)gfx.RDRAM)[(a + 9) ^ 3]) / 127.0f;
        rdp.light[i].dir_z = (float)(((char*)gfx.RDRAM)[(a + 10) ^ 3]) / 127.0f;
        // **

        //rdp.update |= UPDATE_LIGHTS;

        WriteTrace(TraceRDP, TraceDebug, "light: n: %d, r: %.3f, g: %.3f, b: %.3f, x: %.3f, y: %.3f, z: %.3f",
            i, rdp.light[i].r, rdp.light[i].g, rdp.light[i].b,
            rdp.light_vector[i][0], rdp.light_vector[i][1], rdp.light_vector[i][2]);
        break;

    case 0x9E:  //gSPForceMatrix command. Modification of uc2_movemem:matrix. Gonetz.
    {
        // do not update the combined matrix!
        rdp.update &= ~UPDATE_MULT_MAT;

        uint32_t addr = segoffset(rdp.cmd1) & 0x00FFFFFF;
        load_matrix(rdp.combined, addr);

        addr = rdp.pc[rdp.pc_i] & BMASK;
        rdp.pc[rdp.pc_i] = (addr + 24) & BMASK; //skip next 3 command, b/c they all are part of gSPForceMatrix

        WriteTrace(TraceRDP, TraceVerbose, "{%f,%f,%f,%f}", rdp.combined[0][0], rdp.combined[0][1], rdp.combined[0][2], rdp.combined[0][3]);
        WriteTrace(TraceRDP, TraceVerbose, "{%f,%f,%f,%f}", rdp.combined[1][0], rdp.combined[1][1], rdp.combined[1][2], rdp.combined[1][3]);
        WriteTrace(TraceRDP, TraceVerbose, "{%f,%f,%f,%f}", rdp.combined[2][0], rdp.combined[2][1], rdp.combined[2][2], rdp.combined[2][3]);
        WriteTrace(TraceRDP, TraceVerbose, "{%f,%f,%f,%f}", rdp.combined[3][0], rdp.combined[3][1], rdp.combined[3][2], rdp.combined[3][3]);
    }
    break;

    //next 3 command should never appear since they will be skipped in previous command
    case 0x98:
        WriteTrace(TraceRDP, TraceWarning, "matrix 0 - IGNORED");
        break;

    case 0x9A:
        WriteTrace(TraceRDP, TraceWarning, "matrix 1 - IGNORED");
        break;

    case 0x9C:
        WriteTrace(TraceRDP, TraceWarning, "matrix 2 - IGNORED");
        break;

    default:
        WriteTrace(TraceRDP, TraceWarning, "uc0:movemem unknown (index: 0x%08lx)", (rdp.cmd0 >> 16) & 0xFF);
        WriteTrace(TraceRDP, TraceDebug, "unknown (index: 0x%08lx)", (rdp.cmd0 >> 16) & 0xFF);
    }
}

//
// uc0:displaylist - makes a call to another section of code
//
void uc0_displaylist()
{
    uint32_t addr = segoffset(rdp.cmd1) & 0x00FFFFFF;

    // This fixes partially Gauntlet: Legends
    if (addr == rdp.pc[rdp.pc_i] - 8) { WriteTrace(TraceRDP, TraceDebug, "display list not executed!"); return; }

    uint32_t push = (rdp.cmd0 >> 16) & 0xFF; // push the old location?

    WriteTrace(TraceRDP, TraceDebug, "uc0:displaylist: %08lx, push:%s", addr, push ? "no" : "yes");
    WriteTrace(TraceRDP, TraceDebug, " (seg %d, offset %08lx)", (rdp.cmd1 >> 24) & 0x0F, rdp.cmd1 & 0x00FFFFFF);

    switch (push)
    {
    case 0: // push
        if (rdp.pc_i >= 9)
        {
            WriteTrace(TraceRDP, TraceWarning, "** DL stack overflow **");
            return;
        }
        rdp.pc_i++;  // go to the next PC in the stack
        rdp.pc[rdp.pc_i] = addr;  // jump to the address
        break;

    case 1: // no push
        rdp.pc[rdp.pc_i] = addr;  // just jump to the address
        break;

    default:
        WriteTrace(TraceRDP, TraceWarning, "Unknown displaylist operation");
    }
}

//
// tri1 - renders a triangle
//
void uc0_tri1()
{
    WriteTrace(TraceRDP, TraceDebug, "uc0:tri1 #%d - %d, %d, %d", rdp.tri_n,
        ((rdp.cmd1 >> 16) & 0xFF) / 10,
        ((rdp.cmd1 >> 8) & 0xFF) / 10,
        (rdp.cmd1 & 0xFF) / 10);

    gfxVERTEX *vtx[3] = {
        &rdp.vtx(((rdp.cmd1 >> 16) & 0xFF) / 10),
        &rdp.vtx(((rdp.cmd1 >> 8) & 0xFF) / 10),
        &rdp.vtx((rdp.cmd1 & 0xFF) / 10)
    };
    if (g_settings->hacks(CSettings::hack_Makers))
    {
        rdp.force_wrap = FALSE;
        for (int i = 0; i < 3; i++)
        {
            if (vtx[i]->ou < 0.0f || vtx[i]->ov < 0.0f)
            {
                rdp.force_wrap = TRUE;
                break;
            }
        }
    }
    rsp_tri1(vtx);
}

//
// uc0:enddl - ends a call made by uc0:displaylist
//
void uc0_enddl()
{
    WriteTrace(TraceRDP, TraceDebug, "uc0:enddl");

    if (rdp.pc_i == 0)
    {
        WriteTrace(TraceRDP, TraceDebug, "RDP end");

        // Halt execution here
        rdp.halt = true;
    }

    rdp.pc_i--;
}

void uc0_culldl()
{
    uint8_t vStart = (uint8_t)((rdp.cmd0 & 0x00FFFFFF) / 40) & 0xF;
    uint8_t vEnd = (uint8_t)(rdp.cmd1 / 40) & 0x0F;
    uint32_t cond = 0;

    WriteTrace(TraceRDP, TraceDebug, "uc0:culldl start: %d, end: %d", vStart, vEnd);

    if (vEnd < vStart) return;
    for (uint16_t i = vStart; i <= vEnd; i++)
    {
        gfxVERTEX &v = rdp.vtx(i);
        // Check if completely off the screen (quick frustrum clipping for 90 FOV)
        if (v.x >= -v.w)
            cond |= 0x01;
        if (v.x <= v.w)
            cond |= 0x02;
        if (v.y >= -v.w)
            cond |= 0x04;
        if (v.y <= v.w)
            cond |= 0x08;
        if (v.w >= 0.1f)
            cond |= 0x10;

        if (cond == 0x1F)
            return;
    }

    WriteTrace(TraceRDP, TraceDebug, " - ");  // specify that the enddl is not a real command
    uc0_enddl();
}

void uc0_popmatrix()
{
    WriteTrace(TraceRDP, TraceDebug, "uc0:popmatrix");

    uint32_t param = rdp.cmd1;

    switch (param)
    {
    case 0: // modelview
        modelview_pop();
        break;

    case 1: // projection, can't
        break;

    default:
        WriteTrace(TraceRDP, TraceWarning, "Unknown uc0:popmatrix command: 0x%08lx", param);
    }
}

void uc6_obj_sprite();

void uc0_modifyvtx(uint8_t where, uint16_t vtx, uint32_t val)
{
    gfxVERTEX &v = rdp.vtx(vtx);

    switch (where)
    {
    case 0:
        uc6_obj_sprite();
        break;

    case 0x10:    // RGBA
        v.r = (uint8_t)(val >> 24);
        v.g = (uint8_t)((val >> 16) & 0xFF);
        v.b = (uint8_t)((val >> 8) & 0xFF);
        v.a = (uint8_t)(val & 0xFF);
        v.shade_mod = 0;

        WriteTrace(TraceRDP, TraceDebug, "RGBA: %d, %d, %d, %d", v.r, v.g, v.b, v.a);
        break;

    case 0x14:    // ST
    {
        float scale = rdp.Persp_en ? 0.03125f : 0.015625f;
        v.ou = (float)((short)(val >> 16)) * scale;
        v.ov = (float)((short)(val & 0xFFFF)) * scale;
        v.uv_calculated = 0xFFFFFFFF;
        v.uv_scaled = 1;
    }
    WriteTrace(TraceRDP, TraceDebug, "u/v: (%04lx, %04lx), (%f, %f)", (short)(val >> 16), (short)(val & 0xFFFF),
        v.ou, v.ov);
    break;

    case 0x18:    // XY screen
    {
        float scr_x = (float)((short)(val >> 16)) / 4.0f;
        float scr_y = (float)((short)(val & 0xFFFF)) / 4.0f;
        v.screen_translated = 2;
        v.sx = scr_x * rdp.scale_x + rdp.offset_x;
        v.sy = scr_y * rdp.scale_y + rdp.offset_y;
        if (v.w < 0.01f)
        {
            v.w = 1.0f;
            v.oow = 1.0f;
            v.z_w = 1.0f;
        }
        v.sz = rdp.view_trans[2] + v.z_w * rdp.view_scale[2];

        v.scr_off = 0;
        if (scr_x < 0) v.scr_off |= 1;
        if (scr_x > rdp.vi_width) v.scr_off |= 2;
        if (scr_y < 0) v.scr_off |= 4;
        if (scr_y > rdp.vi_height) v.scr_off |= 8;
        if (v.w < 0.1f) v.scr_off |= 16;

        WriteTrace(TraceRDP, TraceDebug, "x/y: (%f, %f)", scr_x, scr_y);
    }
    break;

    case 0x1C:    // Z screen
    {
        float scr_z = (float)((short)(val >> 16));
        v.z_w = (scr_z - rdp.view_trans[2]) / rdp.view_scale[2];
        v.z = v.z_w * v.w;
        WriteTrace(TraceRDP, TraceDebug, "z: %f", scr_z);
    }
    break;

    default:
        WriteTrace(TraceRDP, TraceDebug, "UNKNOWN");
        break;
    }
}

//
// uc0:moveword - moves a word to someplace, like the segment pointers
//
void uc0_moveword()
{
    WriteTrace(TraceRDP, TraceDebug, "uc0:moveword ");

    // Find which command this is (lowest byte of cmd0)
    switch (rdp.cmd0 & 0xFF)
    {
    case 0x00:
        WriteTrace(TraceRDP, TraceWarning, "matrix - IGNORED");
        break;

    case 0x02:
        rdp.num_lights = ((rdp.cmd1 - 0x80000000) >> 5) - 1;  // inverse of equation
        if (rdp.num_lights > 8) rdp.num_lights = 0;

        rdp.update |= UPDATE_LIGHTS;
        WriteTrace(TraceRDP, TraceDebug, "numlights: %d", rdp.num_lights);
        break;

    case 0x04:
        if (((rdp.cmd0 >> 8) & 0xFFFF) == 0x04)
        {
            rdp.clip_ratio = sqrt((float)rdp.cmd1);
            rdp.update |= UPDATE_VIEWPORT;
        }
        WriteTrace(TraceRDP, TraceDebug, "clip %08lx, %08lx", rdp.cmd0, rdp.cmd1);
        break;

    case 0x06:  // segment
        WriteTrace(TraceRDP, TraceDebug, "segment: %08lx -> seg%d", rdp.cmd1, (rdp.cmd0 >> 10) & 0x0F);
        if ((rdp.cmd1&BMASK) < BMASK)
            rdp.segment[(rdp.cmd0 >> 10) & 0x0F] = rdp.cmd1;
        break;

    case 0x08:
    {
        rdp.fog_multiplier = (short)(rdp.cmd1 >> 16);
        rdp.fog_offset = (short)(rdp.cmd1 & 0x0000FFFF);
        WriteTrace(TraceRDP, TraceDebug, "fog: multiplier: %f, offset: %f", rdp.fog_multiplier, rdp.fog_offset);
    }
    break;

    case 0x0a:  // moveword LIGHTCOL
    {
        int n = (rdp.cmd0 & 0xE000) >> 13;
        WriteTrace(TraceRDP, TraceDebug, "lightcol light:%d, %08lx", n, rdp.cmd1);

        rdp.light[n].r = (float)((rdp.cmd1 >> 24) & 0xFF) / 255.0f;
        rdp.light[n].g = (float)((rdp.cmd1 >> 16) & 0xFF) / 255.0f;
        rdp.light[n].b = (float)((rdp.cmd1 >> 8) & 0xFF) / 255.0f;
        rdp.light[n].a = 255;
    }
    break;

    case 0x0c:
    {
        uint16_t val = (uint16_t)((rdp.cmd0 >> 8) & 0xFFFF);
        uint16_t vtx = val / 40;
        uint8_t where = val % 40;
        uc0_modifyvtx(where, vtx, rdp.cmd1);
        WriteTrace(TraceRDP, TraceDebug, "uc0:modifyvtx: vtx: %d, where: 0x%02lx, val: %08lx - ", vtx, where, rdp.cmd1);
    }
    break;

    case 0x0e:
        WriteTrace(TraceRDP, TraceDebug, "perspnorm - IGNORED");
        break;

    default:
        WriteTrace(TraceRDP, TraceWarning, "uc0:moveword unknown (index: 0x%08lx)", rdp.cmd0 & 0xFF);
    }
}

void uc0_texture()
{
    int tile = (rdp.cmd0 >> 8) & 0x07;
    if (tile == 7 && g_settings->hacks(CSettings::hack_Supercross))
    {
        tile = 0; //fix for supercross 2000
    }
    rdp.mipmap_level = (rdp.cmd0 >> 11) & 0x07;
    uint32_t on = (rdp.cmd0 & 0xFF);
    rdp.cur_tile = tile;

    if (on)
    {
        uint16_t s = (uint16_t)((rdp.cmd1 >> 16) & 0xFFFF);
        uint16_t t = (uint16_t)(rdp.cmd1 & 0xFFFF);

        TILE *tmp_tile = &rdp.tiles(tile);
        tmp_tile->on = 1;
        tmp_tile->org_s_scale = s;
        tmp_tile->org_t_scale = t;
        tmp_tile->s_scale = (float)(s + 1) / 65536.0f;
        tmp_tile->t_scale = (float)(t + 1) / 65536.0f;
        tmp_tile->s_scale /= 32.0f;
        tmp_tile->t_scale /= 32.0f;

        rdp.update |= UPDATE_TEXTURE;

        WriteTrace(TraceRDP, TraceDebug, "uc0:texture: tile: %d, mipmap_lvl: %d, on: %d, s_scale: %f, t_scale: %f", tile, rdp.mipmap_level, on, tmp_tile->s_scale, tmp_tile->t_scale);
    }
    else
    {
        WriteTrace(TraceRDP, TraceDebug, "uc0:texture skipped b/c of off");
        rdp.tiles(tile).on = 0;
    }
}

void uc0_setothermode_h()
{
    WriteTrace(TraceRDP, TraceDebug, "uc0:setothermode_h: ");

    int shift, len;
    if (g_settings->ucode() == CSettings::ucode_F3DEX2 || g_settings->ucode() == CSettings::ucode_CBFD)
    {
        len = (rdp.cmd0 & 0xFF) + 1;
        shift = 32 - ((rdp.cmd0 >> 8) & 0xFF) - len;
    }
    else
    {
        shift = (rdp.cmd0 >> 8) & 0xFF;
        len = rdp.cmd0 & 0xFF;
    }

    uint32_t mask = 0;
    int i = len;
    for (; i; i--)
        mask = (mask << 1) | 1;
    mask <<= shift;

    rdp.cmd1 &= mask;
    rdp.othermode_h &= ~mask;
    rdp.othermode_h |= rdp.cmd1;

    if (mask & 0x00000030)  // alpha dither mode
    {
        rdp.alpha_dither_mode = (rdp.othermode_h >> 4) & 0x3;
        WriteTrace(TraceRDP, TraceDebug, "alpha dither mode: %s", str_dither[rdp.alpha_dither_mode]);
    }

    if (mask & 0x000000C0)  // rgb dither mode
    {
        uint32_t dither_mode = (rdp.othermode_h >> 6) & 0x3;
        WriteTrace(TraceRDP, TraceDebug, "rgb dither mode: %s", str_dither[dither_mode]);
    }

    if (mask & 0x00003000)  // filter mode
    {
        rdp.filter_mode = (int)((rdp.othermode_h & 0x00003000) >> 12);
        rdp.update |= UPDATE_TEXTURE;
        WriteTrace(TraceRDP, TraceDebug, "filter mode: %s", str_filter[rdp.filter_mode]);
    }

    if (mask & 0x0000C000)  // tlut mode
    {
        rdp.tlut_mode = (uint8_t)((rdp.othermode_h & 0x0000C000) >> 14);
        WriteTrace(TraceRDP, TraceDebug, "tlut mode: %s", str_tlut[rdp.tlut_mode]);
    }

    if (mask & 0x00300000)  // cycle type
    {
        rdp.cycle_mode = (uint8_t)((rdp.othermode_h & 0x00300000) >> 20);
        rdp.update |= UPDATE_ZBUF_ENABLED;
        WriteTrace(TraceRDP, TraceDebug, "cycletype: %d", rdp.cycle_mode);
    }

    if (mask & 0x00010000)  // LOD enable
    {
        rdp.LOD_en = (rdp.othermode_h & 0x00010000) ? TRUE : FALSE;
        WriteTrace(TraceRDP, TraceDebug, "LOD_en: %d", rdp.LOD_en);
    }

    if (mask & 0x00080000)  // Persp enable
    {
        if (rdp.persp_supported)
            rdp.Persp_en = (rdp.othermode_h & 0x00080000) ? TRUE : FALSE;
        WriteTrace(TraceRDP, TraceDebug, "Persp_en: %d", rdp.Persp_en);
    }

    uint32_t unk = mask & 0x0FFC60F0F;
    if (unk)  // unknown portions, LARGE
    {
        WriteTrace(TraceRDP, TraceDebug, "UNKNOWN PORTIONS: shift: %d, len: %d, unknowns: %08lx", shift, len, unk);
    }
}

void uc0_setothermode_l()
{
    WriteTrace(TraceRDP, TraceDebug, "uc0:setothermode_l ");

    int shift, len;
    if (g_settings->ucode() == CSettings::ucode_F3DEX2 || g_settings->ucode() == CSettings::ucode_CBFD)
    {
        len = (rdp.cmd0 & 0xFF) + 1;
        shift = 32 - ((rdp.cmd0 >> 8) & 0xFF) - len;
        if (shift < 0) shift = 0;
    }
    else
    {
        len = rdp.cmd0 & 0xFF;
        shift = (rdp.cmd0 >> 8) & 0xFF;
    }

    uint32_t mask = 0;
    int i = len;
    for (; i; i--)
        mask = (mask << 1) | 1;
    mask <<= shift;

    rdp.cmd1 &= mask;
    rdp.othermode_l &= ~mask;
    rdp.othermode_l |= rdp.cmd1;

    if (mask & 0x00000003)  // alpha compare
    {
        rdp.acmp = rdp.othermode_l & 0x00000003;
        WriteTrace(TraceRDP, TraceDebug, "alpha compare %s", ACmp[rdp.acmp]);
        rdp.update |= UPDATE_ALPHA_COMPARE;
    }

    if (mask & 0x00000004)  // z-src selection
    {
        rdp.zsrc = (rdp.othermode_l & 0x00000004) >> 2;
        WriteTrace(TraceRDP, TraceDebug, "z-src sel: %s", str_zs[rdp.zsrc]);
        WriteTrace(TraceRDP, TraceDebug, "z-src sel: %08lx", rdp.zsrc);
        rdp.update |= UPDATE_ZBUF_ENABLED;
    }

    if (mask & 0xFFFFFFF8)  // rendermode / blender bits
    {
        rdp.update |= UPDATE_FOG_ENABLED; //if blender has no fog bits, fog must be set off
        rdp.render_mode_changed |= rdp.rm ^ rdp.othermode_l;
        rdp.rm = rdp.othermode_l;
        if (g_settings->flame_corona() && (rdp.rm == 0x00504341)) //hack for flame's corona
        {
            rdp.othermode_l |= 0x00000010;
        }
        WriteTrace(TraceRDP, TraceDebug, "rendermode: %08lx", rdp.othermode_l);  // just output whole othermode_l
    }

    // there is not one setothermode_l that's not handled :)
}

void uc0_setgeometrymode()
{
    rdp.geom_mode |= rdp.cmd1;
    WriteTrace(TraceRDP, TraceDebug, "uc0:setgeometrymode %08lx; result: %08lx", rdp.cmd1, rdp.geom_mode);

    if (rdp.cmd1 & 0x00000001)  // Z-Buffer enable
    {
        if (!(rdp.flags & ZBUF_ENABLED))
        {
            rdp.flags |= ZBUF_ENABLED;
            rdp.update |= UPDATE_ZBUF_ENABLED;
        }
    }
    if (rdp.cmd1 & 0x00001000)  // Front culling
    {
        if (!(rdp.flags & CULL_FRONT))
        {
            rdp.flags |= CULL_FRONT;
            rdp.update |= UPDATE_CULL_MODE;
        }
    }
    if (rdp.cmd1 & 0x00002000)  // Back culling
    {
        if (!(rdp.flags & CULL_BACK))
        {
            rdp.flags |= CULL_BACK;
            rdp.update |= UPDATE_CULL_MODE;
        }
    }

    //Added by Gonetz
    if (rdp.cmd1 & 0x00010000)      // Fog enable
    {
        if (!(rdp.flags & FOG_ENABLED))
        {
            rdp.flags |= FOG_ENABLED;
            rdp.update |= UPDATE_FOG_ENABLED;
        }
    }
}

void uc0_cleargeometrymode()
{
    WriteTrace(TraceRDP, TraceDebug, "uc0:cleargeometrymode %08lx", rdp.cmd1);

    rdp.geom_mode &= (~rdp.cmd1);

    if (rdp.cmd1 & 0x00000001)  // Z-Buffer enable
    {
        if (rdp.flags & ZBUF_ENABLED)
        {
            rdp.flags ^= ZBUF_ENABLED;
            rdp.update |= UPDATE_ZBUF_ENABLED;
        }
    }
    if (rdp.cmd1 & 0x00001000)  // Front culling
    {
        if (rdp.flags & CULL_FRONT)
        {
            rdp.flags ^= CULL_FRONT;
            rdp.update |= UPDATE_CULL_MODE;
        }
    }
    if (rdp.cmd1 & 0x00002000)  // Back culling
    {
        if (rdp.flags & CULL_BACK)
        {
            rdp.flags ^= CULL_BACK;
            rdp.update |= UPDATE_CULL_MODE;
        }
    }

    //Added by Gonetz
    if (rdp.cmd1 & 0x00010000)      // Fog enable
    {
        if (rdp.flags & FOG_ENABLED)
        {
            rdp.flags ^= FOG_ENABLED;
            rdp.update |= UPDATE_FOG_ENABLED;
        }
    }
}

void uc0_line3d()
{
    uint32_t v0 = ((rdp.cmd1 >> 16) & 0xff) / 10;
    uint32_t v1 = ((rdp.cmd1 >> 8) & 0xff) / 10;
    uint16_t width = (uint16_t)(rdp.cmd1 & 0xFF) + 3;

    gfxVERTEX *vtx[3] = {
        &rdp.vtx(v1),
        &rdp.vtx(v0),
        &rdp.vtx(v0)
    };
    uint32_t cull_mode = (rdp.flags & CULLMASK) >> CULLSHIFT;
    rdp.flags |= CULLMASK;
    rdp.update |= UPDATE_CULL_MODE;
    rsp_tri1(vtx, width);
    rdp.flags ^= CULLMASK;
    rdp.flags |= cull_mode << CULLSHIFT;
    rdp.update |= UPDATE_CULL_MODE;

    WriteTrace(TraceRDP, TraceDebug, "uc0:line3d v0:%d, v1:%d, width:%d", v0, v1, width);
}

void uc0_tri4()
{
    // c0: 0000 0123, c1: 456789ab
    // becomes: 405 617 829 a3b

    WriteTrace(TraceRDP, TraceDebug, "uc0:tri4");
    WriteTrace(TraceRDP, TraceDebug, " #%d, #%d, #%d, #%d - %d, %d, %d - %d, %d, %d - %d, %d, %d - %d, %d, %d", rdp.tri_n, rdp.tri_n + 1, rdp.tri_n + 2, rdp.tri_n + 3,
        (rdp.cmd1 >> 28) & 0xF,
        (rdp.cmd0 >> 12) & 0xF,
        (rdp.cmd1 >> 24) & 0xF,
        (rdp.cmd1 >> 20) & 0xF,
        (rdp.cmd0 >> 8) & 0xF,
        (rdp.cmd1 >> 16) & 0xF,
        (rdp.cmd1 >> 12) & 0xF,
        (rdp.cmd0 >> 4) & 0xF,
        (rdp.cmd1 >> 8) & 0xF,
        (rdp.cmd1 >> 4) & 0xF,
        (rdp.cmd0 >> 0) & 0xF,
        (rdp.cmd1 >> 0) & 0xF);

    gfxVERTEX *vtx[12] = {
        &rdp.vtx((rdp.cmd1 >> 28) & 0xF),
        &rdp.vtx((rdp.cmd0 >> 12) & 0xF),
        &rdp.vtx((rdp.cmd1 >> 24) & 0xF),
        &rdp.vtx((rdp.cmd1 >> 20) & 0xF),
        &rdp.vtx((rdp.cmd0 >> 8) & 0xF),
        &rdp.vtx((rdp.cmd1 >> 16) & 0xF),
        &rdp.vtx((rdp.cmd1 >> 12) & 0xF),
        &rdp.vtx((rdp.cmd0 >> 4) & 0xF),
        &rdp.vtx((rdp.cmd1 >> 8) & 0xF),
        &rdp.vtx((rdp.cmd1 >> 4) & 0xF),
        &rdp.vtx((rdp.cmd0 >> 0) & 0xF),
        &rdp.vtx((rdp.cmd1 >> 0) & 0xF),
    };

    int updated = 0;

    if (cull_tri(vtx))
        rdp.tri_n++;
    else
    {
        updated = 1;
        update();

        draw_tri(vtx);
        rdp.tri_n++;
    }

    if (cull_tri(vtx + 3))
        rdp.tri_n++;
    else
    {
        if (!updated)
        {
            updated = 1;
            update();
        }

        draw_tri(vtx + 3);
        rdp.tri_n++;
    }

    if (cull_tri(vtx + 6))
        rdp.tri_n++;
    else
    {
        if (!updated)
        {
            updated = 1;
            update();
        }

        draw_tri(vtx + 6);
        rdp.tri_n++;
    }

    if (cull_tri(vtx + 9))
        rdp.tri_n++;
    else
    {
        if (!updated)
        {
            updated = 1;
            update();
        }

        draw_tri(vtx + 9);
        rdp.tri_n++;
    }
}