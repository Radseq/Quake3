/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "tr_cmds.hpp"
#include "tr_image.hpp"
#include "tr_backend.hpp"
#include "tr_shader.hpp"
#include "tr_scene.hpp"
/*
=====================
R_PerformanceCounters
=====================
*/
static void R_PerformanceCounters(void)
{
	if (!r_speeds->integer)
	{
		// clear the counters even if we aren't printing
		Com_Memset(&tr.pc, 0, sizeof(tr.pc));
		Com_Memset(&backEnd.pc, 0, sizeof(backEnd.pc));
		return;
	}

	if (r_speeds->integer == 1)
	{
		ri.Printf(PRINT_ALL, "%i/%i shaders/surfs %i leafs %i verts %i/%i tris %.2f mtex %.2f dc\n",
				  backEnd.pc.c_shaders, backEnd.pc.c_surfaces, tr.pc.c_leafs, backEnd.pc.c_vertexes,
				  backEnd.pc.c_indexes / 3, backEnd.pc.c_totalIndexes / 3,
				  R_SumOfUsedImages() / (1000000.0f), backEnd.pc.c_overDraw / (float)(glConfig.vidWidth * glConfig.vidHeight));
	}
	else if (r_speeds->integer == 2)
	{
		ri.Printf(PRINT_ALL, "(patch) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n",
				  tr.pc.c_sphere_cull_patch_in, tr.pc.c_sphere_cull_patch_clip, tr.pc.c_sphere_cull_patch_out,
				  tr.pc.c_box_cull_patch_in, tr.pc.c_box_cull_patch_clip, tr.pc.c_box_cull_patch_out);
		ri.Printf(PRINT_ALL, "(md3) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n",
				  tr.pc.c_sphere_cull_md3_in, tr.pc.c_sphere_cull_md3_clip, tr.pc.c_sphere_cull_md3_out,
				  tr.pc.c_box_cull_md3_in, tr.pc.c_box_cull_md3_clip, tr.pc.c_box_cull_md3_out);
	}
	else if (r_speeds->integer == 3)
	{
		ri.Printf(PRINT_ALL, "viewcluster: %i\n", tr.viewCluster);
	}
	else if (r_speeds->integer == 4)
	{
		if (backEnd.pc.c_dlightVertexes)
		{
			ri.Printf(PRINT_ALL, "dlight srf:%i  culled:%i  verts:%i  tris:%i\n",
					  tr.pc.c_dlightSurfaces, tr.pc.c_dlightSurfacesCulled,
					  backEnd.pc.c_dlightVertexes, backEnd.pc.c_dlightIndexes / 3);
		}
	}
	else if (r_speeds->integer == 5)
	{
		ri.Printf(PRINT_ALL, "zFar: %.0f\n", tr.viewParms.zFar);
	}
	else if (r_speeds->integer == 6)
	{
		ri.Printf(PRINT_ALL, "flare adds:%i tests:%i renders:%i\n",
				  backEnd.pc.c_flareAdds, backEnd.pc.c_flareTests, backEnd.pc.c_flareRenders);
	}

	Com_Memset(&tr.pc, 0, sizeof(tr.pc));
	Com_Memset(&backEnd.pc, 0, sizeof(backEnd.pc));
}

/*
====================
R_IssueRenderCommands
====================
*/
static void R_IssueRenderCommands(void)
{
	renderCommandList_t *cmdList;

	cmdList = &backEndData->commands;

	// add an end-of-list command
	*(int *)(cmdList->cmds + cmdList->used) = RC_END_OF_LIST;

	// clear it out, in case this is a sync and not a buffer flip
	cmdList->used = 0;

	if (backEnd.screenshotMask == 0)
	{
		if (ri.CL_IsMinimized())
			return; // skip backend when minimized
		if (backEnd.throttle)
			return; // or throttled on demand
	}
	else
	{
		if (ri.CL_IsMinimized() && !RE_CanMinimize())
		{
			backEnd.screenshotMask = 0;
			return;
		}
	}

	// actually start the commands going
	if (!r_skipBackEnd->integer)
	{
		// let it start on the new batch
		RB_ExecuteRenderCommands(cmdList->cmds);
	}
}

/*
============
R_GetCommandBufferReserved

make sure there is enough command space
============
*/
static void *R_GetCommandBufferReserved(int bytes, int reservedBytes)
{
	renderCommandList_t *cmdList;

	cmdList = &backEndData->commands;
	bytes = PAD(bytes, sizeof(void *));

	// always leave room for the end of list command
	if (cmdList->used + bytes + sizeof(int) + reservedBytes > MAX_RENDER_COMMANDS)
	{
		if (bytes > MAX_RENDER_COMMANDS - sizeof(int))
		{
			ri.Error(ERR_FATAL, "R_GetCommandBuffer_plus: bad size %i", bytes);
		}
		// if we run out of room, just start dropping commands
		return NULL;
	}

	cmdList->used += bytes;

	return cmdList->cmds + cmdList->used - bytes;
}

/*
=============
R_GetCommandBuffer_plus
returns NULL if there is not enough space for important commands
=============
*/
void *R_GetCommandBuffer_plus(int bytes)
{
	tr.lastRenderCommand = RC_END_OF_LIST;
	return R_GetCommandBufferReserved(bytes, PAD(sizeof(swapBuffersCommand_t), sizeof(void *)));
}

/*
=============
R_AddDrawSurfCmd
=============
*/
void R_AddDrawSurfCmd_plus(drawSurf_t *drawSurfs, int numDrawSurfs)
{
	drawSurfsCommand_t *cmd;

	cmd = static_cast<drawSurfsCommand_t *>(R_GetCommandBuffer_plus(sizeof(*cmd)));
	if (!cmd)
	{
		return;
	}
	cmd->commandId = RC_DRAW_SURFS;

	cmd->drawSurfs = drawSurfs;
	cmd->numDrawSurfs = numDrawSurfs;

	cmd->refdef = tr.refdef;
	cmd->viewParms = tr.viewParms;

	tr.numDrawSurfCmds++;
	if (tr.drawSurfCmd == NULL)
	{
		tr.drawSurfCmd = cmd;
	}
}

/*
=============
RE_SetColor

Passing NULL will set the color to white
=============
*/
void RE_SetColor_plus(const float *rgba)
{
	setColorCommand_t *cmd;

	if (!tr.registered)
	{
		return;
	}
	cmd = static_cast<setColorCommand_t *>(R_GetCommandBuffer_plus(sizeof(*cmd)));
	if (!cmd)
	{
		return;
	}
	cmd->commandId = RC_SET_COLOR;
	if (!rgba)
	{
		rgba = colorWhite;
	}

	cmd->color[0] = rgba[0];
	cmd->color[1] = rgba[1];
	cmd->color[2] = rgba[2];
	cmd->color[3] = rgba[3];
}

/*
=============
RE_StretchPic
=============
*/
void RE_StretchPic_plus(float x, float y, float w, float h,
						float s1, float t1, float s2, float t2, qhandle_t hShader)
{
	stretchPicCommand_t *cmd;

	if (!tr.registered)
	{
		return;
	}
	cmd = static_cast<stretchPicCommand_t *>(R_GetCommandBuffer_plus(sizeof(*cmd)));
	if (!cmd)
	{
		return;
	}
	cmd->commandId = RC_STRETCH_PIC;
	cmd->shader = R_GetShaderByHandle_plus(hShader);
	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
	cmd->s1 = s1;
	cmd->t1 = t1;
	cmd->s2 = s2;
	cmd->t2 = t2;
}

/*
====================
RE_BeginFrame

If running in stereo, RE_BeginFrame will be called twice
for each RE_EndFrame
====================
*/
void RE_BeginFrame_plus(stereoFrame_t stereoFrame)
{
	drawBufferCommand_t *cmd;

	if (!tr.registered)
	{
		return;
	}

	backEnd.doneBloom = false;
	backEnd.color2D.u32 = ~0U;

	tr.frameCount++;
	tr.frameSceneNum = 0;

	if ((cmd = static_cast<drawBufferCommand_t *>(R_GetCommandBuffer_plus(sizeof(*cmd)))) == NULL)
		return;

	cmd->commandId = RC_DRAW_BUFFER;

	tr.lastRenderCommand = RC_DRAW_BUFFER;

	if (glConfig.stereoEnabled)
	{
		if (stereoFrame == STEREO_LEFT)
		{
			cmd->buffer = (int)GL_BACK_LEFT;
		}
		else if (stereoFrame == STEREO_RIGHT)
		{
			cmd->buffer = (int)GL_BACK_RIGHT;
		}
		else
		{
			ri.Error(ERR_FATAL, "RE_BeginFrame: Stereo is enabled, but stereoFrame was %i", stereoFrame);
		}
	}
	else
	{
		if (stereoFrame != STEREO_CENTER)
		{
			ri.Error(ERR_FATAL, "RE_BeginFrame: Stereo is disabled, but stereoFrame was %i", stereoFrame);
		}

		cmd->buffer = 0;
	}

	if (r_fastsky->integer && vk.fastSky)
	{
		if (stereoFrame != STEREO_RIGHT)
		{
			clearColorCommand_t *clrcmd;
			if ((clrcmd = static_cast<clearColorCommand_t *>(R_GetCommandBuffer_plus(sizeof(*clrcmd)))) == NULL)
				return;
			clrcmd->commandId = RC_CLEARCOLOR;
		}
	}

	tr.refdef.stereoFrame = stereoFrame;
}

/*
=============
RE_TakeVideoFrame
=============
*/
void RE_TakeVideoFrame_plus(int width, int height,
							byte *captureBuffer, byte *encodeBuffer, bool motionJpeg)
{
	videoFrameCommand_t *cmd;

	if (!tr.registered)
	{
		return;
	}

	backEnd.screenshotMask |= SCREENSHOT_AVI;

	cmd = &backEnd.vcmd;

	// cmd->commandId = RC_VIDEOFRAME;

	cmd->width = width;
	cmd->height = height;
	cmd->captureBuffer = captureBuffer;
	cmd->encodeBuffer = encodeBuffer;
	cmd->motionJpeg = motionJpeg;
}

void RE_ThrottleBackend_plus()
{
	backEnd.throttle = true;
}

void RE_FinishBloom_plus()
{
	finishBloomCommand_t *cmd;

	if (!tr.registered)
	{
		return;
	}

	cmd = static_cast<finishBloomCommand_t *>(R_GetCommandBuffer_plus(sizeof(*cmd)));
	if (!cmd)
	{
		return;
	}

	cmd->commandId = RC_FINISHBLOOM;
}

bool RE_CanMinimize_plus()
{
	if (vk.fboActive || vk.offscreenRender)
		return true;
	return false;
}

const glconfig_t *RE_GetConfig_plus()
{
	return &glConfig;
}

void RE_VertexLighting_plus(bool allowed)
{
	tr.vertexLightingAllowed = allowed;
}
