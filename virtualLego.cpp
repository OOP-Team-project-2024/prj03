////////////////////////////////////////////////////////////////////////////////
//
// File: virtualLego.cpp
//

// Original Author: Chang-hyeon Park, 
// Modified by Bong-Soo Sohn and Dong-Jun Kim
// 
// Originally programmed for Virtual LEGO. 
// Modified later to program for Virtual Billiard.
//        
////////////////////////////////////////////////////////////////////////////////

#include "d3dUtility.h"
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <cassert>


IDirect3DDevice9* Device = NULL;

// window size
const int Width = 1024;
const int Height = 768;

// 테이블 경계값 정의
const float TABLE_MIN_X = -4.5f;
const float TABLE_MAX_X = 4.5f;
const float TABLE_MIN_Z = -3.0f;
const float TABLE_MAX_Z = 3.0f;


// There are four balls
// initialize the position (coordinate) of each ball (ball0 ~ ball3)
const float spherePos[16][2] = {
    {-2.5f, 0.0f},  // 큐볼 위치 
    {1.0f, 0.0f},   // 삼각형 첫 번째 줄 (꼭대기)
    {1.36f, -0.21f}, {1.36f, 0.21f},  // 삼각형 두 번째 줄
    {1.72f, -0.42f}, {1.72f, 0.0f}, {1.72f, 0.42f},  // 삼각형 세 번째 줄
    {2.08f, -0.63f}, {2.08f, -0.21f}, {2.08f, 0.21f}, {2.08f, 0.63f},  // 삼각형 네 번째 줄
    {2.44f, -0.84f}, {2.44f, -0.42f}, {2.44f, 0.0f}, {2.44f, 0.42f}, {2.44f, 0.84f}  // 삼각형 다섯 번째 줄
};




// -----------------------------------------------------------------------------
// Transform matrices
// -----------------------------------------------------------------------------
D3DXMATRIX g_mWorld;
D3DXMATRIX g_mView;
D3DXMATRIX g_mProj;

#define M_RADIUS 0.21   // ball radius
#define PI 3.14159265
#define M_HEIGHT 0.01
#define DECREASE_RATE 0.9982

// -----------------------------------------------------------------------------
// CSphere class definition
// -----------------------------------------------------------------------------

class CSphere {
private:
    float					center_x, center_y, center_z;
    float                   m_radius;
    float					m_velocity_x;
    float					m_velocity_z;
    D3DXVECTOR3 m_prevPos; // 이전 프레임에서의 위치
    float m_rotationAngle; // 누적 회전 각도

public:
    CSphere(void) : isActive(true)
    {
        D3DXMatrixIdentity(&m_mLocal);
        ZeroMemory(&m_mtrl, sizeof(m_mtrl));
        m_radius = 0;
        m_velocity_x = 0;
        m_velocity_z = 0;
        m_pSphereMesh = NULL;
        m_pTexture = NULL;
    }
    ~CSphere(void) {}

    void deactivate() { isActive = false; }
    bool isActiveBall() const { return isActive; }
    //활성 상태 관리 추가
private:
    bool isActive; // 활성 상태 플래그



public:
    bool create(IDirect3DDevice9* pDevice, LPCSTR textureFileName = NULL, D3DXCOLOR color = d3d::WHITE)
    {
        m_prevPos = getCenter();
        m_rotationAngle = 0.0f;
        if (NULL == pDevice)
            return false;


        if (textureFileName != NULL)
        {
            // 텍스처가 존재할 경우 머티리얼 색상을 흰색으로 설정
            m_mtrl.Ambient = d3d::WHITE;
            m_mtrl.Diffuse = d3d::WHITE;
            m_mtrl.Specular = d3d::WHITE;
        }
        else
        {
            // 텍스처가 없을 경우 기존 색상 사용
            m_mtrl.Ambient = color;
            m_mtrl.Diffuse = color;
            m_mtrl.Specular = color;
        }

        m_mtrl.Emissive = d3d::BLACK;
        m_mtrl.Power = 5.0f;

        if (FAILED(D3DXCreateSphere(pDevice, getRadius(), 50, 50, &m_pSphereMesh, NULL)))
            return false;
        // 텍스처 좌표를 포함하도록 메쉬 클론
        HRESULT hr = m_pSphereMesh->CloneMeshFVF(
            D3DXMESH_MANAGED,
            D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1,
            pDevice,
            &m_pSphereMesh);
        if (FAILED(hr))
            return false;

        // 정점 버퍼를 잠그고 텍스처 좌표 계산
        struct Vertex
        {
            D3DXVECTOR3 position;
            D3DXVECTOR3 normal;
            FLOAT       tu, tv;
        };

        Vertex* vertices = nullptr;
        hr = m_pSphereMesh->LockVertexBuffer(0, (void**)&vertices);
        if (FAILED(hr))
            return false;

        DWORD numVertices = m_pSphereMesh->GetNumVertices();
        for (DWORD i = 0; i < numVertices; ++i)
        {
            D3DXVECTOR3& pos = vertices[i].position;
            FLOAT theta = atan2f(pos.z, pos.x);
            FLOAT phi = acosf(pos.y / getRadius());

            // theta와 phi를 [0, 1] 범위로 정규화
            FLOAT u = (theta + D3DX_PI) / (2 * D3DX_PI);
            FLOAT v = phi / D3DX_PI;

            vertices[i].tu = u;
            vertices[i].tv = v;
        }

        m_pSphereMesh->UnlockVertexBuffer();

        if (textureFileName != NULL)
        {
            HRESULT result = D3DXCreateTextureFromFile(pDevice, textureFileName, &m_pTexture);
            if (FAILED(result))
            {
                MessageBox(NULL, "Failed to load texture", "Error", MB_OK);
                return false;
            }
        }
        return true;
    }


    void destroy(void)
    {
        if (m_pSphereMesh != NULL) {
            m_pSphereMesh->Release();
            m_pSphereMesh = NULL;
        }
        if (m_pTexture != NULL) {
            m_pTexture->Release();
            m_pTexture = NULL;
        }
    }

    void draw(IDirect3DDevice9* pDevice, const D3DXMATRIX& mWorld)
    {
        if (NULL == pDevice)
            return;
        pDevice->SetTransform(D3DTS_WORLD, &mWorld);
        pDevice->MultiplyTransform(D3DTS_WORLD, &m_mLocal);
        pDevice->SetMaterial(&m_mtrl);

        pDevice->SetTexture(0, m_pTexture);

        m_pSphereMesh->DrawSubset(0);

        // After drawing, reset the texture
        pDevice->SetTexture(0, NULL);
    }

    bool CSphere::hasIntersected(CSphere& ball) {
        float dx = this->center_x - ball.center_x;
        float dy = this->center_y - ball.center_y;
        float dz = this->center_z - ball.center_z;
        float distanceSquared = dx * dx + dy * dy + dz * dz;

        float radiusSum = M_RADIUS * 2;

        return distanceSquared <= (radiusSum * radiusSum);
    }

    void hitBy(CSphere& ball)

    
    {
        if (!this->isActive || !ball.isActiveBall()) return; // 비활성화된 공은 무시

        if (this->hasIntersected(ball))
        {
            // Get positions and velocities of the two balls
            D3DXVECTOR3 pos1 = this->getCenter();
            D3DXVECTOR3 pos2 = ball.getCenter();

            float vx1 = this->getVelocity_X();
            float vz1 = this->getVelocity_Z();
            float vx2 = ball.getVelocity_X();
            float vz2 = ball.getVelocity_Z();

            // Calculate normal and tangent vectors
            float dx = pos1.x - pos2.x;
            float dz = pos1.z - pos2.z;
            float distance = sqrt(dx * dx + dz * dz);

            // Normalize the normal vector
            float nx = dx / distance;
            float nz = dz / distance;

            // Tangent vector is perpendicular to the normal vector
            float tx = -nz;
            float tz = nx;

            // Project velocities onto the normal and tangent vectors
            float v1n = nx * vx1 + nz * vz1;
            float v1t = tx * vx1 + tz * vz1;
            float v2n = nx * vx2 + nz * vz2;
            float v2t = tx * vx2 + tz * vz2;

            // Swap normal velocities (elastic collision)
            float temp = v1n;
            v1n = v2n;
            v2n = temp;

            // Convert scalar normal and tangential velocities back to vectors
            vx1 = v1n * nx + v1t * tx;
            vz1 = v1n * nz + v1t * tz;
            vx2 = v2n * nx + v2t * tx;
            vz2 = v2n * nz + v2t * tz;

            // Set new velocities for the balls
            this->setPower(vx1, vz1);
            ball.setPower(vx2, vz2);

            // Separate the balls to prevent sticking
            float overlap = M_RADIUS * 2 - distance;
            float correctionX = overlap / 2 * nx;
            float correctionZ = overlap / 2 * nz;

            this->setCenter(pos1.x + correctionX, pos1.y, pos1.z + correctionZ);
            ball.setCenter(pos2.x - correctionX, pos2.y, pos2.z - correctionZ);
        }
    }

    void ballUpdate(float timeDiff)
    {
        if (!this->isActive) return;

        const float TIME_SCALE = 3.3;
        D3DXVECTOR3 cord = this->getCenter();
        double vx = abs(this->getVelocity_X());
        double vz = abs(this->getVelocity_Z());

        if (vx > 0.01 || vz > 0.01)
        {
            float tX = cord.x + TIME_SCALE * timeDiff * m_velocity_x;
            float tZ = cord.z + TIME_SCALE * timeDiff * m_velocity_z;

            //correction of position of ball
            // Please uncomment this part because this correction of ball position is necessary when a ball collides with a wall
            if (tX >= (4.5 - M_RADIUS))
                tX = 4.5 - M_RADIUS;
            else if (tX <= (-4.5 + M_RADIUS))
                tX = -4.5 + M_RADIUS;
            else if (tZ <= (-3 + M_RADIUS))
                tZ = -3 + M_RADIUS;
            else if (tZ >= (3 - M_RADIUS))
                tZ = 3 - M_RADIUS;

            this->setCenter(tX, cord.y, tZ);

            // 이동 벡터 계산
            D3DXVECTOR3 moveDir = getCenter() - m_prevPos;


        }
        else { this->setPower(0, 0); }
        //this->setPower(this->getVelocity_X() * DECREASE_RATE, this->getVelocity_Z() * DECREASE_RATE);
        double rate = 1 - (1 - DECREASE_RATE) * timeDiff * 400;
        if (rate < 0)
            rate = 0;
        this->setPower(getVelocity_X() * rate, getVelocity_Z() * rate);
    }

    double getVelocity_X() { return this->m_velocity_x; }
    double getVelocity_Z() { return this->m_velocity_z; }

    void setPower(double vx, double vz)
    {
        this->m_velocity_x = vx;
        this->m_velocity_z = vz;
    }

    void setCenter(float x, float y, float z)
    {
        D3DXMATRIX m;
        center_x = x;	center_y = y;	center_z = z;
        D3DXMatrixTranslation(&m, x, y, z);
        setLocalTransform(m);
    }

    float getRadius(void)  const { return (float)(M_RADIUS); }
    const D3DXMATRIX& getLocalTransform(void) const { return m_mLocal; }
    void setLocalTransform(const D3DXMATRIX& mLocal) { m_mLocal = mLocal; }
    D3DXVECTOR3 getCenter(void) const
    {
        D3DXVECTOR3 org(center_x, center_y, center_z);
        return org;
    }

private:
    D3DXMATRIX              m_mLocal;
    D3DMATERIAL9            m_mtrl;
    ID3DXMesh* m_pSphereMesh;
    LPDIRECT3DTEXTURE9 m_pTexture;
};



// -----------------------------------------------------------------------------
// CWall class definition
// -----------------------------------------------------------------------------

class CWall {

private:

    float					m_x;
    float					m_z;
    float                   m_width;
    float                   m_depth;
    float					m_height;

public:
    CWall(void)
    {
        D3DXMatrixIdentity(&m_mLocal);
        ZeroMemory(&m_mtrl, sizeof(m_mtrl));
        m_width = 0;
        m_depth = 0;
        m_pBoundMesh = NULL;
    }
    ~CWall(void) {}
public:
    bool create(IDirect3DDevice9* pDevice, float ix, float iz, float iwidth, float iheight, float idepth, D3DXCOLOR color = d3d::WHITE)
    {
        if (NULL == pDevice)
            return false;

        m_mtrl.Ambient = color;
        m_mtrl.Diffuse = color;
        m_mtrl.Specular = color;
        m_mtrl.Emissive = d3d::BLACK;
        m_mtrl.Power = 5.0f;

        m_width = iwidth;
        m_depth = idepth;

        if (FAILED(D3DXCreateBox(pDevice, iwidth, iheight, idepth, &m_pBoundMesh, NULL)))
            return false;
        return true;
    }
    void destroy(void)
    {
        if (m_pBoundMesh != NULL) {
            m_pBoundMesh->Release();
            m_pBoundMesh = NULL;
        }
    }
    void draw(IDirect3DDevice9* pDevice, const D3DXMATRIX& mWorld)
    {
        if (NULL == pDevice)
            return;
        pDevice->SetTransform(D3DTS_WORLD, &mWorld);
        pDevice->MultiplyTransform(D3DTS_WORLD, &m_mLocal);
        pDevice->SetMaterial(&m_mtrl);
        m_pBoundMesh->DrawSubset(0);
    }


    bool hasIntersected(CSphere& ball)
    {
        float leftXBoundary = this->m_x - (this->m_width / 2);
        float rightXBoundary = this->m_x + (this->m_width / 2);

        float frontZBoundary = this->m_z - (this->m_depth / 2);
        float backZBoundary = this->m_z + (this->m_depth / 2);

        float ballX = ball.getCenter().x;
        float ballZ = ball.getCenter().z;

        bool isWithinXBounds = (leftXBoundary <= ballX && ballX <= rightXBoundary);
        bool isWithinZBounds = (frontZBoundary <= ballZ && ballZ <= backZBoundary);

        if (isWithinXBounds || isWithinZBounds) {
            // Colliding vertically
            if (abs(this->m_x - ballX) <= this->m_width / 2 + ball.getRadius() &&
                abs(this->m_z - ballZ) <= this->m_depth / 2 + ball.getRadius()) {
                return true;
            }
        }
        else {
            // Colliding with an edge
        }

        return false;
    }

    void hitBy(CSphere& ball)
    {
        if (hasIntersected(ball))
        {
            float ball_vx = (float)ball.getVelocity_X();
            float ball_vz = (float)ball.getVelocity_Z();

            // 수평
            if (this->m_x == 0.0f)
            {
                ball.setPower(ball_vx, -ball_vz);

            }
            else if (this->m_z == 0.0f)
            {
                // 수직
                ball.setPower(-ball_vx, ball_vz);

            }
        }
    }

    void setPosition(float x, float y, float z)
    {
        D3DXMATRIX m;
        this->m_x = x;
        this->m_z = z;

        D3DXMatrixTranslation(&m, x, y, z);
        setLocalTransform(m);
    }

    float getHeight(void) const { return M_HEIGHT; }



private:
    void setLocalTransform(const D3DXMATRIX& mLocal) { m_mLocal = mLocal; }

    D3DXMATRIX              m_mLocal;
    D3DMATERIAL9            m_mtrl;
    ID3DXMesh* m_pBoundMesh;
};

// -----------------------------------------------------------------------------
// Cpocket class definition
// -----------------------------------------------------------------------------

class CPocket {
private:
    D3DXVECTOR3 m_position; // 구멍의 중심 좌표
    float m_radius;         // 구멍의 반지름

public:

    CPocket() : m_position(D3DXVECTOR3(0.0f, 0.0f, 0.0f)), m_radius(0.0f) {}

    CPocket(D3DXVECTOR3 position, float radius)
        : m_position(position), m_radius(radius) {}

    D3DXVECTOR3 getPosition() const {
        return m_position;
    }

    float getRadius() const {
        return m_radius;
    }

    bool isBallInPocket(const CSphere& ball) const {
        D3DXVECTOR3 ballPos = ball.getCenter();
        float distanceSquared =
            (ballPos.x - m_position.x) * (ballPos.x - m_position.x) +
            (ballPos.z - m_position.z) * (ballPos.z - m_position.z);
        return distanceSquared <= (m_radius * m_radius);
    }

    void draw(IDirect3DDevice9* pDevice, const D3DXMATRIX& mWorld) const {
        if (!pDevice) return;

        D3DXVECTOR3 worldPos = getTransformedPosition(mWorld);
        D3DXMATRIX pocketTransform;
        D3DXMatrixTranslation(&pocketTransform, worldPos.x, worldPos.y, worldPos.z);

        // 월드 변환 적용
        pDevice->SetTransform(D3DTS_WORLD, &pocketTransform);

        // 포켓 렌더링 (기본 검정색 머티리얼)
        D3DMATERIAL9 mtrl;
        ZeroMemory(&mtrl, sizeof(mtrl));
        mtrl.Diffuse = D3DXCOLOR(0, 0, 0, 1);
        mtrl.Ambient = D3DXCOLOR(0, 0, 0, 1);
        pDevice->SetMaterial(&mtrl);

        ID3DXMesh* pocketMesh = NULL;
        if (SUCCEEDED(D3DXCreateSphere(pDevice, m_radius, 20, 20, &pocketMesh, NULL))) {
            pocketMesh->DrawSubset(0);
            pocketMesh->Release();
        }
    }
    // 포켓의 월드 좌표를 계산
    D3DXVECTOR3 getTransformedPosition(const D3DXMATRIX& worldMatrix) const {
        D3DXVECTOR3 transformedPosition;
        D3DXVec3TransformCoord(&transformedPosition, &m_position, &worldMatrix);
        return transformedPosition;
    }

    void setPosition(float x, float y, float z) {
        m_position = D3DXVECTOR3(x, y, z);
    }

    // 현재 위치에서 변위만큼 이동
    void translate(float dx, float dy, float dz) {
        m_position.x += dx;
        m_position.y += dy;
        m_position.z += dz;
    }
};


// 전역 변수에 pockets 추가
const int NUM_POCKETS = 6;
CPocket pockets[NUM_POCKETS] = {
    CPocket(D3DXVECTOR3(-4.5f, 0.0f, 3.0f), 0.3f),  // 상단 왼쪽
    CPocket(D3DXVECTOR3(0.0f, 0.0f, 3.0f), 0.3f),   // 상단 중앙
    CPocket(D3DXVECTOR3(4.5f, 0.0f, 3.0f), 0.3f),   // 상단 오른쪽
    CPocket(D3DXVECTOR3(-4.5f, 0.0f, -3.0f), 0.3f), // 하단 왼쪽
    CPocket(D3DXVECTOR3(0.0f, 0.0f, -3.0f), 0.3f),  // 하단 중앙
    CPocket(D3DXVECTOR3(4.5f, 0.0f, -3.0f), 0.3f)   // 하단 오른쪽
};


// -----------------------------------------------------------------------------
// CLight class definition
// -----------------------------------------------------------------------------

class CLight {
public:
    CLight(void)
    {
        static DWORD i = 0;
        m_index = i++;
        D3DXMatrixIdentity(&m_mLocal);
        ::ZeroMemory(&m_lit, sizeof(m_lit));
        m_pMesh = NULL;
        m_bound._center = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        m_bound._radius = 0.0f;
    }
    ~CLight(void) {}
public:
    bool create(IDirect3DDevice9* pDevice, const D3DLIGHT9& lit, float radius = 0.1f)
    {
        if (NULL == pDevice)
            return false;
        if (FAILED(D3DXCreateSphere(pDevice, radius, 10, 10, &m_pMesh, NULL)))
            return false;

        m_bound._center = lit.Position;
        m_bound._radius = radius;

        m_lit.Type = lit.Type;
        m_lit.Diffuse = lit.Diffuse;
        m_lit.Specular = lit.Specular;
        m_lit.Ambient = lit.Ambient;
        m_lit.Position = lit.Position;
        m_lit.Direction = lit.Direction;
        m_lit.Range = lit.Range;
        m_lit.Falloff = lit.Falloff;
        m_lit.Attenuation0 = lit.Attenuation0;
        m_lit.Attenuation1 = lit.Attenuation1;
        m_lit.Attenuation2 = lit.Attenuation2;
        m_lit.Theta = lit.Theta;
        m_lit.Phi = lit.Phi;
        return true;
    }
    void destroy(void)
    {
        if (m_pMesh != NULL) {
            m_pMesh->Release();
            m_pMesh = NULL;
        }
    }
    bool setLight(IDirect3DDevice9* pDevice, const D3DXMATRIX& mWorld)
    {
        if (NULL == pDevice)
            return false;

        D3DXVECTOR3 pos(m_bound._center);
        D3DXVec3TransformCoord(&pos, &pos, &m_mLocal);
        D3DXVec3TransformCoord(&pos, &pos, &mWorld);
        m_lit.Position = pos;

        pDevice->SetLight(m_index, &m_lit);
        pDevice->LightEnable(m_index, TRUE);
        return true;
    }

    void draw(IDirect3DDevice9* pDevice)
    {
        if (NULL == pDevice)
            return;
        D3DXMATRIX m;
        D3DXMatrixTranslation(&m, m_lit.Position.x, m_lit.Position.y, m_lit.Position.z);
        pDevice->SetTransform(D3DTS_WORLD, &m);
        pDevice->SetMaterial(&d3d::WHITE_MTRL);
        m_pMesh->DrawSubset(0);
    }

    D3DXVECTOR3 getPosition(void) const { return D3DXVECTOR3(m_lit.Position); }

private:
    DWORD               m_index;
    D3DXMATRIX          m_mLocal;
    D3DLIGHT9           m_lit;
    ID3DXMesh* m_pMesh;
    d3d::BoundingSphere m_bound;
};


// -----------------------------------------------------------------------------
// Global variables
// -----------------------------------------------------------------------------
CWall	g_legoPlane;
CWall	g_legowall[4];
CSphere	g_sphere[16];
CSphere	g_target_blueball;
CLight	g_light;

double g_camera_pos[3] = { 0.0, 5.0, -8.0 };

// -----------------------------------------------------------------------------
// Functions
// -----------------------------------------------------------------------------


void destroyAllLegoBlock(void)
{
}

// initialization
bool Setup()
{
    int i;

    D3DXMatrixIdentity(&g_mWorld);
    D3DXMatrixIdentity(&g_mView);
    D3DXMatrixIdentity(&g_mProj);

    // create plane and set the position
    if (false == g_legoPlane.create(Device, -1, -1, 9, 0.03f, 6, d3d::GREEN)) return false;
    g_legoPlane.setPosition(0.0f, -0.0006f / 5, 0.0f);

    // create walls and set the position. note that there are four walls
    if (false == g_legowall[0].create(Device, -1, -1, 9, 0.3f, 0.12f, d3d::DARKRED)) return false;
    g_legowall[0].setPosition(0.0f, 0.12f, 3.06f);
    if (false == g_legowall[1].create(Device, -1, -1, 9, 0.3f, 0.12f, d3d::DARKRED)) return false;
    g_legowall[1].setPosition(0.0f, 0.12f, -3.06f);
    if (false == g_legowall[2].create(Device, -1, -1, 0.12f, 0.3f, 6.24f, d3d::DARKRED)) return false;
    g_legowall[2].setPosition(4.56f, 0.12f, 0.0f);
    if (false == g_legowall[3].create(Device, -1, -1, 0.12f, 0.3f, 6.24f, d3d::DARKRED)) return false;
    g_legowall[3].setPosition(-4.56f, 0.12f, 0.0f);

    // create four balls and set the position
    for (i = 0; i < 16; i++) {
        char textureFileName[256];
        sprintf(textureFileName, "image\\Ball%d.jpg", i);
        if (false == g_sphere[i].create(Device, textureFileName)) return false;
        g_sphere[i].setCenter(spherePos[i][0], (float)M_RADIUS, spherePos[i][1]);
        g_sphere[i].setPower(0, 0);
    }

    // create blue ball for set direction
    if (false == g_target_blueball.create(Device, NULL, d3d::BLUE)) return false;
    g_target_blueball.setCenter(.0f, (float)M_RADIUS, .0f);

    // light setting 
    D3DLIGHT9 lit;
    ::ZeroMemory(&lit, sizeof(lit));
    lit.Type = D3DLIGHT_POINT;
    lit.Diffuse = d3d::WHITE;
    lit.Specular = d3d::WHITE * 0.9f;
    lit.Ambient = d3d::WHITE * 0.9f;
    lit.Position = D3DXVECTOR3(0.0f, 3.0f, 0.0f);
    lit.Range = 100.0f;
    lit.Attenuation0 = 0.0f;
    lit.Attenuation1 = 0.9f;
    lit.Attenuation2 = 0.0f;
    if (false == g_light.create(Device, lit))
        return false;

    // Position and aim the camera.
    D3DXVECTOR3 pos(0.0f, 5.0f, -8.0f);
    D3DXVECTOR3 target(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 up(0.0f, 2.0f, 0.0f);
    D3DXMatrixLookAtLH(&g_mView, &pos, &target, &up);
    Device->SetTransform(D3DTS_VIEW, &g_mView);

    // Set the projection matrix.
    D3DXMatrixPerspectiveFovLH(&g_mProj, D3DX_PI / 4,
        (float)Width / (float)Height, 1.0f, 100.0f);
    Device->SetTransform(D3DTS_PROJECTION, &g_mProj);

    // Set render states.
    Device->SetRenderState(D3DRS_LIGHTING, TRUE);
    Device->SetRenderState(D3DRS_SPECULARENABLE, TRUE);
    Device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);

    Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    Device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
    Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    g_light.setLight(Device, g_mWorld);
    return true;
}

void Cleanup(void)
{
    g_legoPlane.destroy();
    for (int i = 0; i < 4; i++) {
        g_legowall[i].destroy();
    }
    destroyAllLegoBlock();
    g_light.destroy();
}


// timeDelta represents the time between the current image frame and the last image frame.
// the distance of moving balls should be "velocity * timeDelta"
bool Display(float timeDelta) {
    if (Device) {
        Device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00afafaf, 1.0f, 0);
        Device->BeginScene();

        // Ball updates and pocket collision
        for (int i = 0; i < 16; i++) {
            if (!g_sphere[i].isActiveBall()) continue; // 비활성화된 공 건너뛰기

            for (const auto& pocket : pockets) {
                if (pocket.isBallInPocket(g_sphere[i])) {
                    g_sphere[i].deactivate(); // 공 비활성화
                    g_sphere[i].setCenter(-999.0f, -999.0f, -999.0f); // 물리적으로 접근 불가능한 위치
                    g_sphere[i].setPower(0, 0); // 속도 제거

                    if (i == 0) { // 큐볼이 구멍에 빠졌다면
                        g_sphere[0].setCenter(-2.5f, M_RADIUS, 0.0f); // 큐볼 초기화
                        g_sphere[0].setPower(0, 0);
                    }
                    break; // 더 이상 처리할 필요 없음
                }


                g_sphere[i].ballUpdate(timeDelta);

                for (int j = 0; j < 4; j++) {
                    g_legowall[j].hitBy(g_sphere[i]);
                }
            }

            // Ball-to-ball collisions
            for (int i = 0; i < 16; i++) {
                for (int j = i + 1; j < 16; j++) {
                    g_sphere[i].hitBy(g_sphere[j]);
                }
            }

            // Draw plane, walls, pockets, and active balls
            g_legoPlane.draw(Device, g_mWorld);
            for (int i = 0; i < 4; i++) {
                g_legowall[i].draw(Device, g_mWorld);
            }
            for (const auto& pocket : pockets) {
                pocket.draw(Device, g_mWorld);
            }
            for (int i = 0; i < 16; i++) {
                if (g_sphere[i].isActiveBall()) {
                    g_sphere[i].draw(Device, g_mWorld);
                }
            }
            g_target_blueball.draw(Device, g_mWorld);
            g_light.draw(Device);

            Device->EndScene();
            Device->Present(0, 0, 0, 0);
            Device->SetTexture(0, NULL);
        }

        return true;
    }
}


    LRESULT CALLBACK d3d::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static bool wire = false;
    static bool isReset = true;
    static int old_x = 0;
    static int old_y = 0;
    static enum { WORLD_MOVE, LIGHT_MOVE, BLOCK_MOVE } move = WORLD_MOVE;

    switch (msg) {
    case WM_DESTROY:
    {
        ::PostQuitMessage(0);
        break;
    }
    case WM_KEYDOWN:
    {
        switch (wParam) {
        case VK_ESCAPE:
            ::DestroyWindow(hwnd);
            break;
        case VK_RETURN:
            if (NULL != Device) {
                wire = !wire;
                Device->SetRenderState(D3DRS_FILLMODE,
                    (wire ? D3DFILL_WIREFRAME : D3DFILL_SOLID));
            }
            break;

        case VK_SPACE:

            D3DXVECTOR3 targetpos = g_target_blueball.getCenter();
            D3DXVECTOR3	whitepos = g_sphere[0].getCenter();
            double theta = acos(sqrt(pow(targetpos.x - whitepos.x, 2)) / sqrt(pow(targetpos.x - whitepos.x, 2) +

                pow(targetpos.z - whitepos.z, 2)));
            if (targetpos.z - whitepos.z <= 0 && targetpos.x - whitepos.x >= 0) { theta = -theta; }
            if (targetpos.z - whitepos.z >= 0 && targetpos.x - whitepos.x <= 0) { theta = PI - theta; }
            if (targetpos.z - whitepos.z <= 0 && targetpos.x - whitepos.x <= 0) { theta = PI + theta; }

            double distance = sqrt(pow(targetpos.x - whitepos.x, 2) + pow(targetpos.z - whitepos.z, 2));
            g_sphere[0].setPower(distance * cos(theta), distance * sin(theta));

            break;

        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        int new_x = LOWORD(lParam);
        int new_y = HIWORD(lParam);
        float dx;
        float dy;

        if (LOWORD(wParam) & MK_LBUTTON) {

            if (isReset) {
                isReset = false;
            }
            else {
                D3DXVECTOR3 vDist;
                D3DXVECTOR3 vTrans;
                D3DXMATRIX mTrans;
                D3DXMATRIX mX;
                D3DXMATRIX mY;

                switch (move) {
                case WORLD_MOVE:
                    dx = (old_x - new_x) * 0.01f;
                    dy = (old_y - new_y) * 0.01f;


                    D3DXMatrixRotationY(&mX, dx);
                    D3DXMatrixRotationX(&mY, dy);
                    g_mWorld = g_mWorld * mX * mY;

                    break;
                }
            }

            old_x = new_x;
            old_y = new_y;

        }
        else {
            isReset = true;

            if (LOWORD(wParam) & MK_RBUTTON) {
                dx = (old_x - new_x) * (-0.007f);
                dy = (old_y - new_y) * 0.007f;

                // 기존 위치 가져오기
                D3DXVECTOR3 coord3d = g_target_blueball.getCenter();
                float new_x_pos = coord3d.x + dx;
                float new_z_pos = coord3d.z + dy;

                // 테이블 크기에 맞춘 경계 제한
                const float TABLE_MIN_X = -4.5f + M_RADIUS; // 반지름만큼 여유를 둠
                const float TABLE_MAX_X = 4.5f - M_RADIUS;
                const float TABLE_MIN_Z = -3.0f + M_RADIUS;
                const float TABLE_MAX_Z = 3.0f - M_RADIUS;

                // 위치 제한
                new_x_pos = std::max(TABLE_MIN_X, std::min(TABLE_MAX_X, new_x_pos));
                new_z_pos = std::max(TABLE_MIN_Z, std::min(TABLE_MAX_Z, new_z_pos));

                // 제한된 위치로 파란 공 이동
                g_target_blueball.setCenter(new_x_pos, coord3d.y, new_z_pos);
            }

            old_x = new_x;
            old_y = new_y;
            break;
        }

    return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

    int WINAPI WinMain(HINSTANCE hinstance,
        HINSTANCE prevInstance,
        PSTR cmdLine,
        int showCmd)
{
    srand(static_cast<unsigned int>(time(NULL)));

    if (!d3d::InitD3D(hinstance,
        Width, Height, true, D3DDEVTYPE_HAL, &Device))
    {
        ::MessageBox(0, "InitD3D() - FAILED", 0, 0);
        return 0;
    }

    if (!Setup())
    {
        ::MessageBox(0, "Setup() - FAILED", 0, 0);
        return 0;
    }

    d3d::EnterMsgLoop(Display);

    Cleanup();

    Device->Release();

    return 0;
}