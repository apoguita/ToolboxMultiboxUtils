#pragma once

#include "Headers.h"

#include <iostream>
#include <windows.h>

#include <Windows.h>
#include "imgui.h"

constexpr auto ALTITUDE_UNKNOWN = std::numeric_limits<float>::max();

using namespace DirectX;

GW::Vec3f MouseWorldPos;
GW::Vec2f MouseScreenPos;

struct GlobalMouseClass {
    void SetMouseWorldPos(float x, float y, float z) {
        MouseWorldPos.x = x;
        MouseWorldPos.y = y;
        MouseWorldPos.z = z;
    };
    GW::Vec3f GetMouseWorldPos() {
        return MouseWorldPos;
    };

    void SetMouseCoords(float x, float y) {
        MouseScreenPos.x = x;
        MouseScreenPos.y = y;
    }

    GW::Vec2f GetMouseCoords() {
        return MouseScreenPos;
    };
};

class OverlayClass {
private:
    XMMATRIX CreateViewMatrix(const XMFLOAT3& eye_pos, const XMFLOAT3& look_at_pos, const XMFLOAT3& up_direction);
    XMMATRIX CreateProjectionMatrix(float fov, float aspect_ratio, float near_plane, float far_plane);
    GW::Vec3f GetWorldToScreen(const GW::Vec3f& world_position, const XMMATRIX& mat_view, const XMMATRIX& mat_proj, float viewport_width, float viewport_height);
    void GetScreenToWorld();
    float findZ(float x, float y, float pz);
public:
        GW::Vec3f WorldToScreen(const GW::Vec3f& world_position);
        GW::Vec3f ScreenToWorld();
        void BeginDraw();
        void EndDraw();
        void DrawFilledPoly(ImDrawList* drawList, const ImVec2& center, float radius, ImU32 color, int numSegments);
        void DrawPoly(ImDrawList* drawList, const GW::Vec2f& center, float radius, ImU32 color, int numSegments);
        void DrawPoly3D(ImDrawList* drawList, const GW::Vec3f& center, float radius, ImU32 color, int numSegments, bool autoZ = true);
        void DrawLine(ImDrawList* drawList, const ImVec2& from, const ImVec2& to, ImU32 color, float thickness);
        void DrawLine3D(ImDrawList* drawList, const GW::Vec3f& from, const GW::Vec3f& to, ImU32 color, float thickness);
        void DrawText3D(ImDrawList* drawList, const ImVec2& position, const char* text, ImU32 color);
        void DrawSprite(ImDrawList* drawList, ImTextureID texture, const ImVec2& position, const ImVec2& size, float scaleX, float scaleY);
        void DrawTriangle(ImDrawList* drawList, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 color, float thickness);
        void DrawTriangle3D(ImDrawList* drawList, const GW::Vec3f& p1, const GW::Vec3f& p2, const GW::Vec3f& p3, ImU32 color, float thickness);
        void DrawFilledTriangle(ImDrawList* drawList, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 color);
        void DrawFilledTriangle3D(ImDrawList* drawList, const GW::Vec3f& p1, const GW::Vec3f& p2, const GW::Vec3f& p3, ImU32 color);

        void DrawFlagAll(ImDrawList* drawList, GW::Vec3f pos);
        void DrawFlagHero(ImDrawList* drawList, GW::Vec3f origin);

        void DrawDoubleRowLineFormation(ImDrawList* drawList, const GW::Vec3f& center, float angle, ImU32 color);
};

void OverlayClass::BeginDraw() {
    ImGuiIO& io = ImGui::GetIO();

    // Make the panel cover the whole screen
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    // Create a transparent and click-through panel
    ImGui::Begin("HeroOverlay", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoInputs);

}

void OverlayClass::EndDraw() {

    ImGui::End();
}

XMMATRIX OverlayClass::CreateViewMatrix(const XMFLOAT3& eye_pos, const XMFLOAT3& look_at_pos, const XMFLOAT3& up_direction) {
    XMMATRIX viewMatrix = XMMatrixLookAtLH(XMLoadFloat3(&eye_pos), XMLoadFloat3(&look_at_pos), XMLoadFloat3(&up_direction));
    return viewMatrix;
}

XMMATRIX OverlayClass::CreateProjectionMatrix(float fov, float aspect_ratio, float near_plane, float far_plane) {
    XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(fov, aspect_ratio, near_plane, far_plane);
    return projectionMatrix;
}

// Function to convert world coordinates to screen coordinates
GW::Vec3f OverlayClass::GetWorldToScreen(const GW::Vec3f& world_position, const XMMATRIX& mat_view, const XMMATRIX& mat_proj, float viewport_width, float viewport_height) {
    GW::Vec3f res;

    // Transform the point into camera space
    XMVECTOR pos = XMVectorSet(world_position.x, world_position.y, world_position.z, 1.0f);
    pos = XMVector3TransformCoord(pos, mat_view);

    // Transform the point into projection space
    pos = XMVector3TransformCoord(pos, mat_proj);

    // Perform perspective division
    XMFLOAT4 projected;
    XMStoreFloat4(&projected, pos);
    if (projected.w < 0.1f) return res; // Point is behind the camera

    projected.x /= projected.w;
    projected.y /= projected.w;
    projected.z /= projected.w;

    // Transform to screen space
    res.x = ((projected.x + 1.0f) / 2.0f) * viewport_width;
    res.y = ((-projected.y + 1.0f) / 2.0f) * viewport_height;
    res.z = projected.z;

    return res;
}

GW::Vec3f OverlayClass::WorldToScreen(const GW::Vec3f& world_position) {
    static auto cam = GW::CameraMgr::GetCamera();
    float camX = GW::CameraMgr::GetCamera()->position.x;
    float camY = GW::CameraMgr::GetCamera()->position.y;
    float camZ = GW::CameraMgr::GetCamera()->position.z;


    XMFLOAT3 eyePos = { camX, camY, camZ };
    XMFLOAT3 lookAtPos = { cam->look_at_target.x, cam->look_at_target.y, cam->look_at_target.z };
    XMFLOAT3 upDir = { 0.0f, 0.0f, -1.0f };  // Up Vector

    // Create view matrix
    XMMATRIX viewMatrix = CreateViewMatrix(eyePos, lookAtPos, upDir);

    // Define projection parameters
    float fov = GW::Render::GetFieldOfView();
    float aspectRatio = static_cast<float>(GW::Render::GetViewportWidth()) / static_cast<float>(GW::Render::GetViewportHeight());
    float nearPlane = 0.1f;
    float farPlane = 100000.0f;

    // Create projection matrix
    XMMATRIX projectionMatrix = CreateProjectionMatrix(fov, aspectRatio, nearPlane, farPlane);
    GW::Vec3f res = GetWorldToScreen(world_position, viewMatrix, projectionMatrix, static_cast<float>(GW::Render::GetViewportWidth()), static_cast<float>(GW::Render::GetViewportHeight()));

    return res;

}

void OverlayClass::GetScreenToWorld() {

    GW::GameThread::Enqueue([]() {

        MemMgrClass ptrGetter;
        GlobalMouseClass setmousepos;
        GW::Vec2f* screen_coord = 0;
        uintptr_t address = GW::Scanner::Find("\x8B\xE5\x5D\xC3\x8B\x4F\x10\x83\xC7\x08", "xxxxxxxxxx", 0xC);
        //uintptr_t address = ptrGetter.GetNdcScreenCoordsPtr();
        if (Verify(address))
        {
            screen_coord = *reinterpret_cast<GW::Vec2f**>(address);
        }
        else { setmousepos.SetMouseWorldPos(0, 0, 0); return; }

        address = GW::Scanner::Find("\xD9\x5D\x14\xD9\x45\x14\x83\xEC\x0C", "xxxxxxxx", -0x2F);
        if (address)
        {
            ScreenToWorldPoint_Func = (ScreenToWorldPoint_pt)address;
        }
        else { setmousepos.SetMouseWorldPos(0, 0, 0); return; }

        address = GW::Scanner::Find("\xff\x75\x08\xd9\x5d\xfc\xff\x77\x7c", "xxxxxxxxx", -0x27);
        if (address) {
            MapIntersect_Func = (MapIntersect_pt)address;
        }
        else { setmousepos.SetMouseWorldPos(0, 0, 0); return; }

        GW::Vec3f origin;
        GW::Vec3f p2;
        float unk1 = ScreenToWorldPoint_Func(&origin, screen_coord->x, screen_coord->y, 0);
        float unk2 = ScreenToWorldPoint_Func(&p2, screen_coord->x, screen_coord->y, 1);
        GW::Vec3f dir = p2 - origin;
        GW::Vec3f ray_dir = GW::Normalize(dir);
        GW::Vec3f hit_point;
        int layer = 0;
        uint32_t ret = MapIntersect_Func(&origin, &ray_dir, &hit_point, &layer); //needs to run in game thread

        if (ret) { setmousepos.SetMouseWorldPos(hit_point.x, hit_point.y, hit_point.z); }
        else { setmousepos.SetMouseWorldPos(0, 0, 0); }

        });

}

GW::Vec3f OverlayClass::ScreenToWorld() {
    GetScreenToWorld();
    GlobalMouseClass mousepos;
    GW::Vec3f ret_mouse_coords = mousepos.GetMouseWorldPos();
    return ret_mouse_coords;

}

// Function to draw a line
void OverlayClass::DrawLine(ImDrawList* drawList, const ImVec2& from, const ImVec2& to, ImU32 color, float thickness) {
    drawList->AddLine(from, to, color, thickness);
}

void OverlayClass::DrawLine3D(ImDrawList* drawList, const GW::Vec3f& from, const GW::Vec3f& to, ImU32 color, float thickness) {
    GW::Vec3f from3D = WorldToScreen(from);
    GW::Vec3f to3D = WorldToScreen(to);

    ImVec2 fromProjected;
    ImVec2 toProjected;

    fromProjected.x = from3D.x;
    fromProjected.y = from3D.y;

    toProjected.x = to3D.x;
    toProjected.y = to3D.y;

    drawList->AddLine(fromProjected, toProjected, color, thickness);
}

// Function to draw text
void OverlayClass::DrawText3D(ImDrawList* drawList, const ImVec2& position, const char* text, ImU32 color) {
    drawList->AddText(position, color, text);
}

// Function to draw a sprite with scaling
void OverlayClass::DrawSprite(ImDrawList* drawList, ImTextureID texture, const ImVec2& position, const ImVec2& size, float scaleX, float scaleY) {
    ImVec2 scaledSize(size.x * scaleX, size.y * scaleY);
    ImVec2 uv0(0.0f, 0.0f);
    ImVec2 uv1(1.0f, 1.0f);
    drawList->AddImage(texture, position, ImVec2(position.x + scaledSize.x, position.y + scaledSize.y), uv0, uv1);
}

// Function to draw a triangle
void OverlayClass::DrawTriangle(ImDrawList* drawList, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 color, float thickness) {
    drawList->AddLine(p1, p2, color, thickness);
    drawList->AddLine(p2, p3, color, thickness);
    drawList->AddLine(p3, p1, color, thickness);
}

void OverlayClass::DrawTriangle3D(ImDrawList* drawList, const  GW::Vec3f& p1, const  GW::Vec3f& p2, const  GW::Vec3f& p3, ImU32 color, float thickness) {
    GW::Vec3f p13D = WorldToScreen(p1);
    GW::Vec3f p23D = WorldToScreen(p2);
    GW::Vec3f p33D = WorldToScreen(p3);

    ImVec2 p1Projected;
    ImVec2 p2Projected;
    ImVec2 p3Projected;

    p1Projected.x = p13D.x;
    p1Projected.y = p13D.y;
    p2Projected.x = p23D.x;
    p2Projected.y = p23D.y;
    p3Projected.x = p33D.x;
    p3Projected.y = p33D.y;

    drawList->AddLine(p1Projected, p2Projected, color, thickness);
    drawList->AddLine(p2Projected, p3Projected, color, thickness);
    drawList->AddLine(p3Projected, p1Projected, color, thickness);
}

// Function to draw a filled triangle
void OverlayClass::DrawFilledTriangle(ImDrawList* drawList, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 color) {
    drawList->AddTriangleFilled(p1, p2, p3, color);
}

void OverlayClass::DrawFilledTriangle3D(ImDrawList* drawList, const GW::Vec3f& p1, const GW::Vec3f& p2, const GW::Vec3f& p3, ImU32 color) {
    GW::Vec3f p13D = WorldToScreen(p1);
    GW::Vec3f p23D = WorldToScreen(p2);
    GW::Vec3f p33D = WorldToScreen(p3);

    ImVec2 p1Projected;
    ImVec2 p2Projected;
    ImVec2 p3Projected;

    p1Projected.x = p13D.x;
    p1Projected.y = p13D.y;
    p2Projected.x = p23D.x;
    p2Projected.y = p23D.y;
    p3Projected.x = p33D.x;
    p3Projected.y = p33D.y;


    drawList->AddTriangleFilled(p1Projected, p2Projected, p3Projected, color);
}

float OverlayClass::findZ(float x, float y, float pz) {

    float altitude = ALTITUDE_UNKNOWN;
    unsigned int cur_altitude = 0u;
    float z = pz - (pz * 0.05);// adds an offset more of z to look for unleveled planes

    // in order to properly query altitudes, we have to use the pathing map
    // to determine the number of Z planes in the current map.
    const GW::PathingMapArray* pathing_map = GW::Map::GetPathingMap();
    if (pathing_map != nullptr) {
        const size_t pmap_size = pathing_map->size();
        if (pmap_size > 0) {

            GW::Map::QueryAltitude({ x, y, cur_altitude }, 0.1f, altitude);
            if (altitude < z) {
                // recall that the Up camera component is inverted
                z = altitude;
            }

        }
    }
    return z;
}

// Draw circle in Perspective
void OverlayClass::DrawPoly3D(ImDrawList* drawList, const GW::Vec3f& center, float radius, ImU32 color, int numSegments, bool autoZ) {
    const float segmentRadian = 2 * 3.14159265358979323846f / numSegments; // Calculate radians per segment
    ImVec2 prevPoint(center.x + radius, center.y); // Starting point for the first segment
    GW::Vec3f prevPoint3D;
    prevPoint3D.x = prevPoint.x;
    prevPoint3D.y = prevPoint.y;
    prevPoint3D.z = autoZ ? findZ(center.x, center.y, center.z) : center.z;

    GW::Vec3f prevPointtransformed3D = WorldToScreen(prevPoint3D);
    ImVec2 prevPointTransformed(prevPointtransformed3D.x, prevPointtransformed3D.y);

    // Draw the circle using line segments
    for (int i = 1; i <= numSegments; ++i) {
        float angle = segmentRadian * i;
        ImVec2 currentPoint(center.x + radius * cos(angle), center.y + radius * sin(angle));
        GW::Vec3f currentPoint3D;
        currentPoint3D.x = currentPoint.x;
        currentPoint3D.y = currentPoint.y;
        currentPoint3D.z = autoZ ? findZ(currentPoint3D.x, currentPoint3D.y, center.z) : center.z;

        GW::Vec3f currentPointtransformed3D = WorldToScreen(currentPoint3D);
        ImVec2 currentPointTransformed(currentPointtransformed3D.x, currentPointtransformed3D.y);


        drawList->AddLine(prevPointTransformed, currentPointTransformed, color);
        prevPoint = currentPoint;

        prevPoint3D.x = prevPoint.x;
        prevPoint3D.y = prevPoint.y;
        prevPoint3D.z = autoZ ? findZ(prevPoint3D.x, prevPoint3D.y, center.z) : center.z;

        prevPointtransformed3D = WorldToScreen(prevPoint3D);
        prevPointTransformed.x = prevPointtransformed3D.x;
        prevPointTransformed.y = prevPointtransformed3D.y;
    }
}


void OverlayClass::DrawFlagAll(ImDrawList* drawList, GW::Vec3f origin) {

    //FLAG ALL DRAWING

       GW::Vec3f from;
       from.x = origin.x;
       from.y = origin.y;
       from.z = findZ(origin.x, origin.y, origin.z);

       GW::Vec3f to;
       to.x = origin.x;
       to.y = origin.y;
       to.z = from.z - 150.0f;
       DrawLine3D(drawList, from, to, IM_COL32(0, 255, 0, 255), 3.0f);

       GW::Vec3f p1;
       GW::Vec3f p2;
       GW::Vec3f p3;

       p1 = to;
       p2 = to;
       p2.z = p2.z + 30.0f;
       p3 = to;
       p3.x = p3.x - 50;
       p3.z = p3.z + 15.0f;

       DrawFilledTriangle3D(drawList, p1, p2, p3, IM_COL32(0, 255, 0, 255));
      
       //END FLAG ALL DRAWING
}

void OverlayClass::DrawFlagHero(ImDrawList* drawList, GW::Vec3f origin) {

    //FLAG HERO DRAWING

    GW::Vec3f from;
    from.x = origin.x;
    from.y = origin.y;
    from.z = findZ(origin.x, origin.y, origin.z);

    GW::Vec3f to;
    to.x = origin.x;
    to.y = origin.y;
    to.z = from.z - 150.0f;
    DrawLine3D(drawList, from, to, IM_COL32(0, 255, 0, 255), 3.0f);

    GW::Vec3f p1;
    GW::Vec3f p2;
    GW::Vec3f p3;

    p1 = to;
    p1.x = p1.x + 25.0f;
    p2 = to;
    p2.x = p2.x - 25.0f;
    p3 = to;
    p3.z = p3.z + 50.0f;


    DrawFilledTriangle3D(drawList, p1, p2, p3, IM_COL32(0, 255, 0, 255));

    //END FLAG HERO DRAWING
}


void OverlayClass::DrawDoubleRowLineFormation(ImDrawList* drawList, const GW::Vec3f& center, float angle, ImU32 color) {
    float spacing = GW::Constants::Range::Touch;
    float radius = GW::Constants::Range::Touch / 2;
    //float startX = center.x;
    float startX = center.x - 2 * spacing;  // Adjust for three heroes per row
    float startY = center.y - spacing;

    // Draw first row
    for (int i = 0; i < 3; ++i) {
        GW::Vec3f point;
        point.x = startX + spacing / 2 + i * spacing;
        point.y = startY;

        DrawPoly3D(drawList, point, radius, color, 360);

    }
    
    // Draw second row
    startY += spacing;
    for (int i = 0; i < 4; ++i) {
        GW::Vec3f point;
        point.x = startX + i * spacing;
        point.y = startY;

        DrawPoly3D(drawList, point, radius, color, 360);
    }
    
}

