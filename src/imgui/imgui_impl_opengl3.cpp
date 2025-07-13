// based on imgui_impl_opengl3.cpp of https://github.com/ocornut/imgui

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <stdint.h> // intptr_t

// GL includes
#include "../glad/glad.h"

#define GLSL_VERSION "#version 330\n"

// IMGUI_IMPL_OPENGL_USE_VERTEX_ARRAY is always on
// IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART -> glEnable(GL_PRIMITIVE_RESTART) not used
// IMGUI_IMPL_OPENGL_MAY_HAVE_POLYGON_MODE is always on -> glPolygonMode() not used
// IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_BUFFER_PIXEL_UNPACK -> glBindBuffer(GL_PIXEL_UNPACK_BUFFER...) not used


// OpenGL Data
struct ImGui_ImplOpenGL3_Data
{
    GLuint          GlVersion;               // Extracted at runtime using GL_MAJOR_VERSION, GL_MINOR_VERSION queries (e.g. 320 for GL 3.2)
    char            GlslVersionString[32];   // Specified by user or detected based on compile time GL settings.
    bool            GlProfileIsES2;
    bool            GlProfileIsES3;
    bool            GlProfileIsCompat;
    GLint           GlProfileMask;
    GLint           MaxTextureSize;
    GLuint          ShaderHandle;
    GLint           AttribLocationTex;       // Uniforms location
    GLint           AttribLocationProjMtx;
    GLuint          AttribLocationVtxPos;    // Vertex attributes location
    GLuint          AttribLocationVtxUV;
    GLuint          AttribLocationVtxColor;
    unsigned int    VboHandle, ElementsHandle;
    GLsizeiptr      VertexBufferSize;
    GLsizeiptr      IndexBufferSize;
    bool            HasPolygonMode;
    bool            HasClipOrigin;
    bool            UseBufferSubData;
    ImVector<char>  TempBuffer;

    ImGui_ImplOpenGL3_Data() { memset((void*)this, 0, sizeof(*this)); }
};

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplOpenGL3_Data* ImGui_ImplOpenGL3_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplOpenGL3_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

bool ImGui_ImplOpenGL3_InitLoader()
{
    // we load the OpenGL functions in GuiWindow.cpp
    return true;
}

// Functions
bool ImGui_ImplOpenGL3_Init()
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Initialize loader
    if (!ImGui_ImplOpenGL3_InitLoader())
        return false;

    // Setup backend capabilities flags
    ImGui_ImplOpenGL3_Data* bd = IM_NEW(ImGui_ImplOpenGL3_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_opengl3";

    // Query for GL version (e.g. 320 for GL 3.2)
    GLint major = 0;
    GLint minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    bd->GlVersion = (GLuint)(major * 100 + minor * 10);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &bd->MaxTextureSize);

    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;   // We can honor ImGuiPlatformIO::Textures[] requests during render.

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_TextureMaxWidth = platform_io.Renderer_TextureMaxHeight = (int)bd->MaxTextureSize;

    // Store GLSL version string so we can refer to it later in case we recreate shaders.
    strcpy(bd->GlslVersionString, GLSL_VERSION);

    // Make an arbitrary GL call (we don't actually need the result)
    // IF YOU GET A CRASH HERE: it probably means the OpenGL function loader didn't do its job. Let us know!
    GLint current_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

    // Detect extensions we support
    bd->HasClipOrigin = (bd->GlVersion >= 450);
    GLint num_extensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
    for (GLint i = 0; i < num_extensions; i++)
    {
        const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
        if (extension != nullptr && strcmp(extension, "GL_ARB_clip_control") == 0)
            bd->HasClipOrigin = true;
    }

    return true;
}

void ImGui_ImplOpenGL3_Shutdown()
{
    ImGui_ImplOpenGL3_Data* bd = ImGui_ImplOpenGL3_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
    IM_DELETE(bd);
}

void ImGui_ImplOpenGL3_NewFrame()
{
    ImGui_ImplOpenGL3_Data* bd = ImGui_ImplOpenGL3_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplOpenGL3_Init()?");

    ImGui_ImplOpenGL3_InitLoader(); // Lazily init loader if not already done for e.g. DLL boundaries.

    if (!bd->ShaderHandle)
        if (!ImGui_ImplOpenGL3_CreateDeviceObjects())
            IM_ASSERT(0 && "ImGui_ImplOpenGL3_CreateDeviceObjects() failed!");
}

static void ImGui_ImplOpenGL3_SetupRenderState(ImDrawData* draw_data, int fb_width, int fb_height, GLuint vertex_array_object)
{
    ImGui_ImplOpenGL3_Data* bd = ImGui_ImplOpenGL3_GetBackendData();

    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST);
    //glDisable(GL_PRIMITIVE_RESTART);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Support for GL 4.5 rarely used glClipControl(GL_UPPER_LEFT)
#if defined(GL_CLIP_ORIGIN)
    bool clip_origin_lower_left = true;
    if (bd->HasClipOrigin)
    {
        GLenum current_clip_origin = 0; glGetIntegerv(GL_CLIP_ORIGIN, (GLint*)&current_clip_origin);
        if (current_clip_origin == GL_UPPER_LEFT)
            clip_origin_lower_left = false;
    }
#endif

    // Setup viewport, orthographic projection matrix
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
#if defined(GL_CLIP_ORIGIN)
    if (!clip_origin_lower_left) { float tmp = T; T = B; B = tmp; } // Swap top and bottom if origin is upper left
#endif
    const float ortho_projection[4][4] =
    {
        { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
        { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
        { 0.0f,         0.0f,        -1.0f,   0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
    };
    glUseProgram(bd->ShaderHandle);
    glUniform1i(bd->AttribLocationTex, 0);
    glUniformMatrix4fv(bd->AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);

#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
    if (bd->GlVersion >= 330 || bd->GlProfileIsES3)
        glBindSampler(0, 0); // We use combined texture/sampler state. Applications using GL 3.3 and GL ES 3.0 may set that otherwise.
#endif

    glBindVertexArray(vertex_array_object);

    // Bind vertex/index buffers and setup attributes for ImDrawVert
    glBindBuffer(GL_ARRAY_BUFFER, bd->VboHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bd->ElementsHandle);
    glEnableVertexAttribArray(bd->AttribLocationVtxPos);
    glEnableVertexAttribArray(bd->AttribLocationVtxUV);
    glEnableVertexAttribArray(bd->AttribLocationVtxColor);
    glVertexAttribPointer(bd->AttribLocationVtxPos,   2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, pos));
    glVertexAttribPointer(bd->AttribLocationVtxUV,    2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, uv));
    glVertexAttribPointer(bd->AttribLocationVtxColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, col));
}

// OpenGL3 Render function.
// (this used to be set in io.RenderDrawListsFn and called by ImGui::Render(), but you can now call this directly from your main loop)
// Note that this implementation is little overcomplicated because we are saving/setting up/restoring every OpenGL state explicitly, in order to be able to run within any OpenGL engine that doesn't do so.
void ImGui_ImplOpenGL3_RenderDrawData(ImDrawData* draw_data, std::function<bool (char const *)> filter)
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    ImGui_ImplOpenGL3_InitLoader(); // Lazily init loader if not already done for e.g. DLL boundaries.

    ImGui_ImplOpenGL3_Data* bd = ImGui_ImplOpenGL3_GetBackendData();

    // Catch up with texture updates. Most of the times, the list will have 1 element with an OK status, aka nothing to do.
    // (This almost always points to ImGui::GetPlatformIO().Textures[] but is part of ImDrawData to allow overriding or disabling texture updates).
    if (draw_data->Textures != nullptr)
        for (ImTextureData* tex : *draw_data->Textures)
            if (tex->Status != ImTextureStatus_OK)
                ImGui_ImplOpenGL3_UpdateTexture(tex);

    // Backup GL state
    // removed because we assume default state escept for viewport and scissor box
    GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
    GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);

    // Setup desired GL state
    // Recreate the VAO every time (this is to easily allow multiple GL contexts to be rendered to. VAO are not shared among GL contexts)
    // The renderer would actually work without any VAO bound, but then our VertexAttrib calls would overwrite the default one currently bound.
    GLuint vertex_array_object = 0;
    glGenVertexArrays(1, &vertex_array_object);
    ImGui_ImplOpenGL3_SetupRenderState(draw_data, fb_width, fb_height, vertex_array_object);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    for (const ImDrawList* draw_list : draw_data->CmdLists)
    {
        // added: check if window should rendered
        if (!filter(draw_list->_OwnerName))
            continue;

        // Upload vertex/index buffers
        // - OpenGL drivers are in a very sorry state nowadays....
        //   During 2021 we attempted to switch from glBufferData() to orphaning+glBufferSubData() following reports
        //   of leaks on Intel GPU when using multi-viewports on Windows.
        // - After this we kept hearing of various display corruptions issues. We started disabling on non-Intel GPU, but issues still got reported on Intel.
        // - We are now back to using exclusively glBufferData(). So bd->UseBufferSubData IS ALWAYS FALSE in this code.
        //   We are keeping the old code path for a while in case people finding new issues may want to test the bd->UseBufferSubData path.
        // - See https://github.com/ocornut/imgui/issues/4468 and please report any corruption issues.
        const GLsizeiptr vtx_buffer_size = (GLsizeiptr)draw_list->VtxBuffer.Size * (int)sizeof(ImDrawVert);
        const GLsizeiptr idx_buffer_size = (GLsizeiptr)draw_list->IdxBuffer.Size * (int)sizeof(ImDrawIdx);
        glBufferData(GL_ARRAY_BUFFER, vtx_buffer_size, (const GLvoid*)draw_list->VtxBuffer.Data, GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx_buffer_size, (const GLvoid*)draw_list->IdxBuffer.Data, GL_STREAM_DRAW);

        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplOpenGL3_SetupRenderState(draw_data, fb_width, fb_height, vertex_array_object);
                else
                    pcmd->UserCallback(draw_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle (Y is inverted in OpenGL)
                glScissor((int)clip_min.x, (int)((float)fb_height - clip_max.y), (int)(clip_max.x - clip_min.x), (int)(clip_max.y - clip_min.y));

                // Bind texture, Draw
                glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->GetTexID());
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_VTX_OFFSET
                if (bd->GlVersion >= 320)
                    GL_CALL(glDrawElementsBaseVertex(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void*)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx)), (GLint)pcmd->VtxOffset));
                else
#endif
                glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void*)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx)));
            }
        }
    }

    // Destroy the temporary VAO
    glDeleteVertexArrays(1, &vertex_array_object);

    // Restore modified GL state
    // we restore default state
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
    if (bd->GlVersion >= 330 || bd->GlProfileIsES3)
        glBindSampler(0, 0);
#endif
    glActiveTexture(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBlendEquationSeparate(GL_FUNC_ADD,  GL_FUNC_ADD);
    glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    //glDisable(GL_PRIMITIVE_RESTART);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
    (void)bd; // Not all compilation paths use this
}

static void ImGui_ImplOpenGL3_DestroyTexture(ImTextureData* tex)
{
    GLuint gl_tex_id = (GLuint)(intptr_t)tex->TexID;
    glDeleteTextures(1, &gl_tex_id);

    // Clear identifiers and mark as destroyed (in order to allow e.g. calling InvalidateDeviceObjects while running)
    tex->SetTexID(ImTextureID_Invalid);
    tex->SetStatus(ImTextureStatus_Destroyed);
}

void ImGui_ImplOpenGL3_UpdateTexture(ImTextureData* tex)
{
    if (tex->Status == ImTextureStatus_WantCreate)
    {
        // Create and upload new texture to graphics system
        //IMGUI_DEBUG_LOG("UpdateTexture #%03d: WantCreate %dx%d\n", tex->UniqueID, tex->Width, tex->Height);
        IM_ASSERT(tex->TexID == 0 && tex->BackendUserData == nullptr);
        IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);
        const void* pixels = tex->GetPixels();
        GLuint gl_texture_id = 0;

        // Upload texture to graphics system
        // (Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling)
        GLint last_texture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
        glGenTextures(1, &gl_texture_id);
        glBindTexture(GL_TEXTURE_2D, gl_texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef GL_UNPACK_ROW_LENGTH // Not on WebGL/ES
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex->Width, tex->Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        // Store identifiers
        tex->SetTexID((ImTextureID)(intptr_t)gl_texture_id);
        tex->SetStatus(ImTextureStatus_OK);

        // Restore state
        glBindTexture(GL_TEXTURE_2D, last_texture);
    }
    else if (tex->Status == ImTextureStatus_WantUpdates)
    {
        // Update selected blocks. We only ever write to textures regions which have never been used before!
        // This backend choose to use tex->Updates[] but you can use tex->UpdateRect to upload a single region.
        GLint last_texture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);

        GLuint gl_tex_id = (GLuint)(intptr_t)tex->TexID;
        glBindTexture(GL_TEXTURE_2D, gl_tex_id);
#if 0// GL_UNPACK_ROW_LENGTH // Not on WebGL/ES
        GL_CALL(glPixelStorei(GL_UNPACK_ROW_LENGTH, tex->Width));
        for (ImTextureRect& r : tex->Updates)
            GL_CALL(glTexSubImage2D(GL_TEXTURE_2D, 0, r.x, r.y, r.w, r.h, GL_RGBA, GL_UNSIGNED_BYTE, tex->GetPixelsAt(r.x, r.y)));
        GL_CALL(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
#else
        // GL ES doesn't have GL_UNPACK_ROW_LENGTH, so we need to (A) copy to a contiguous buffer or (B) upload line by line.
        ImGui_ImplOpenGL3_Data* bd = ImGui_ImplOpenGL3_GetBackendData();
        for (ImTextureRect& r : tex->Updates)
        {
            const int src_pitch = r.w * tex->BytesPerPixel;
            bd->TempBuffer.resize(r.h * src_pitch);
            char* out_p = bd->TempBuffer.Data;
            for (int y = 0; y < r.h; y++, out_p += src_pitch)
                memcpy(out_p, tex->GetPixelsAt(r.x, r.y + y), src_pitch);
            IM_ASSERT(out_p == bd->TempBuffer.end());
            glTexSubImage2D(GL_TEXTURE_2D, 0, r.x, r.y, r.w, r.h, GL_RGBA, GL_UNSIGNED_BYTE, bd->TempBuffer.Data);
        }
#endif
        tex->SetStatus(ImTextureStatus_OK);
        glBindTexture(GL_TEXTURE_2D, last_texture); // Restore state
    }
    else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0)
        ImGui_ImplOpenGL3_DestroyTexture(tex);
}


// If you get an error please report on github. You may try different GL context version or GLSL version. See GL<>GLSL version table at the top of this file.
static bool CheckShader(GLuint handle, const char* desc)
{
    ImGui_ImplOpenGL3_Data* bd = ImGui_ImplOpenGL3_GetBackendData();
    GLint status = 0, log_length = 0;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if ((GLboolean)status == GL_FALSE)
        fprintf(stderr, "ERROR: ImGui_ImplOpenGL3_CreateDeviceObjects: failed to compile %s! With GLSL: %s\n", desc, bd->GlslVersionString);
    if (log_length > 1)
    {
        ImVector<char> buf;
        buf.resize((int)(log_length + 1));
        glGetShaderInfoLog(handle, log_length, nullptr, (GLchar*)buf.begin());
        fprintf(stderr, "%s\n", buf.begin());
    }
    return (GLboolean)status == GL_TRUE;
}

// If you get an error please report on GitHub. You may try different GL context version or GLSL version.
static bool CheckProgram(GLuint handle, const char* desc)
{
    ImGui_ImplOpenGL3_Data* bd = ImGui_ImplOpenGL3_GetBackendData();
    GLint status = 0, log_length = 0;
    glGetProgramiv(handle, GL_LINK_STATUS, &status);
    glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if ((GLboolean)status == GL_FALSE)
        fprintf(stderr, "ERROR: ImGui_ImplOpenGL3_CreateDeviceObjects: failed to link %s! With GLSL %s\n", desc, bd->GlslVersionString);
    if (log_length > 1)
    {
        ImVector<char> buf;
        buf.resize((int)(log_length + 1));
        glGetProgramInfoLog(handle, log_length, nullptr, (GLchar*)buf.begin());
        fprintf(stderr, "%s\n", buf.begin());
    }
    return (GLboolean)status == GL_TRUE;
}

bool ImGui_ImplOpenGL3_CreateDeviceObjects()
{
    ImGui_ImplOpenGL3_Data* bd = ImGui_ImplOpenGL3_GetBackendData();

    const GLchar* vertex_shader_glsl_410_core =
        "layout (location = 0) in vec2 Position;\n"
        "layout (location = 1) in vec2 UV;\n"
        "layout (location = 2) in vec4 Color;\n"
        "uniform mat4 ProjMtx;\n"
        "out vec2 Frag_UV;\n"
        "out vec4 Frag_Color;\n"
        "void main()\n"
        "{\n"
        "    Frag_UV = UV;\n"
        "    Frag_Color = Color;\n"
        "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
        "}\n";

    const GLchar* fragment_shader_glsl_410_core =
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "uniform sampler2D Texture;\n"
        "layout (location = 0) out vec4 Out_Color;\n"
        "void main()\n"
        "{\n"
        "    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
        "}\n";

    // Select shaders matching our GLSL versions
    const GLchar* vertex_shader = vertex_shader_glsl_410_core;
    const GLchar* fragment_shader = fragment_shader_glsl_410_core;

    // Create shaders
    const GLchar* vertex_shader_with_version[2] = { bd->GlslVersionString, vertex_shader };
    GLuint vert_handle;
    vert_handle = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_handle, 2, vertex_shader_with_version, nullptr);
    glCompileShader(vert_handle);
    if (!CheckShader(vert_handle, "vertex shader"))
        return false;

    const GLchar* fragment_shader_with_version[2] = { bd->GlslVersionString, fragment_shader };
    GLuint frag_handle;
    frag_handle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_handle, 2, fragment_shader_with_version, nullptr);
    glCompileShader(frag_handle);
    if (!CheckShader(frag_handle, "fragment shader"))
        return false;

    // Link
    bd->ShaderHandle = glCreateProgram();
    glAttachShader(bd->ShaderHandle, vert_handle);
    glAttachShader(bd->ShaderHandle, frag_handle);
    glLinkProgram(bd->ShaderHandle);
    if (!CheckProgram(bd->ShaderHandle, "shader program"))
        return false;

    glDetachShader(bd->ShaderHandle, vert_handle);
    glDetachShader(bd->ShaderHandle, frag_handle);
    glDeleteShader(vert_handle);
    glDeleteShader(frag_handle);

    bd->AttribLocationTex = glGetUniformLocation(bd->ShaderHandle, "Texture");
    bd->AttribLocationProjMtx = glGetUniformLocation(bd->ShaderHandle, "ProjMtx");
    bd->AttribLocationVtxPos = (GLuint)glGetAttribLocation(bd->ShaderHandle, "Position");
    bd->AttribLocationVtxUV = (GLuint)glGetAttribLocation(bd->ShaderHandle, "UV");
    bd->AttribLocationVtxColor = (GLuint)glGetAttribLocation(bd->ShaderHandle, "Color");

    // Create buffers
    glGenBuffers(1, &bd->VboHandle);
    glGenBuffers(1, &bd->ElementsHandle);

    // Restore modified GL state
    // we set to default
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

void ImGui_ImplOpenGL3_DestroyDeviceObjects()
{
    ImGui_ImplOpenGL3_Data* bd = ImGui_ImplOpenGL3_GetBackendData();
    if (bd->VboHandle)      { glDeleteBuffers(1, &bd->VboHandle); bd->VboHandle = 0; }
    if (bd->ElementsHandle) { glDeleteBuffers(1, &bd->ElementsHandle); bd->ElementsHandle = 0; }
    if (bd->ShaderHandle)   { glDeleteProgram(bd->ShaderHandle); bd->ShaderHandle = 0; }

    // Destroy all textures
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
        if (tex->RefCount == 1)
            ImGui_ImplOpenGL3_DestroyTexture(tex);
}
