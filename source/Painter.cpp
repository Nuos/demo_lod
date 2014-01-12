
#include <cassert>
#include <cmath>

#include <QKeyEvent>
#include <QVector4D>
#include <QRectF>
#include <QPointF>
#include <QSizeF>
#include <QPolygonF>

#include "Terrain.h"
#include "FileAssociatedShader.h"
#include "FileAssociatedTexture.h"
#include "Camera.h"
#include "Canvas.h"
#include "PatchedTerrain.h"

#include "ScreenAlignedQuad.h"

#include "Painter.h"

namespace
{
}


Painter::Painter()
: m_camera(nullptr)
, m_quad(nullptr)
, m_terrain(nullptr)
, m_height (-1)
, m_normals(-1)
, m_diffuse(-1)
, m_detail (-1)
, m_detailn(-1)
, m_drawLineStrips(false)
, m_precission(1.f)
, m_level(0)
, m_debug(false)
, m_maximumDetail(6)
, m_clearColor(0.6f, 0.8f, 0.94f, 1.f)
{
    setMode(PaintMode0);
}

Painter::~Painter()
{
    qDeleteAll(m_programs);
    qDeleteAll(m_shaders);

    FileAssociatedTexture::clean(*this);
}

bool Painter::initialize()
{
    initializeOpenGLFunctions();

    // Note: As always, feel free to change/modify parameters .. as long as its possible to
    // see the terrain and navigation basically works, everything is ok.

    glClearColor(1.f, 1.f, 1.f, 0.f);

    m_quad = new ScreenAlignedQuad(*this);

    QOpenGLShaderProgram * program;

    program = createBasicShaderProgram("data/terrain_4_1.vert", "data/terrain_4_1.frag");
    m_programs[PaintMode1] = program;
    program->bindAttributeLocation("a_vertex", 0);
    program->link();

    // load resources - goto https://moodle.hpi3d.de/course/view.php?id=58 and download either
    // the Tree Canyon Terrain or the complete Terrain Pack. Feel free to modify/exchange the data and extends/scales/offsets.

    m_height = FileAssociatedTexture::getOrCreate2Dus16("data/tree_canyon_h.raw", 2048, 2048, *this, &m_heights);
//    m_yOffset = -1.f;
    m_yOffset = -2.f;
    m_yScale  =  3.f;

    m_normals = FileAssociatedTexture::getOrCreate2D("data/tree_canyon_n.png", *this);
    m_diffuse = FileAssociatedTexture::getOrCreate2D("data/tree_canyon_c.png", *this);
    m_detail  = FileAssociatedTexture::getOrCreate2D("data/moss_detail_c.png",   *this, GL_REPEAT, GL_REPEAT);
    m_detailn = FileAssociatedTexture::getOrCreate2D("data/moss_detail_n.png", *this, GL_REPEAT, GL_REPEAT);
    //m_detailc = FileAssociatedTexture::getOrCreate2D("data/moss-detail-c.png", *this, GL_REPEAT, GL_REPEAT);

    // Task_4_1 - ToDo Begin

    // Pick your maximum tree traversal depth/level (or recursion depth)
    // This should limit the minimum extend of a single patch in respect to the terrain
    // and your approach (4x4 or 2x2 subdivisions)
    m_level = 2;
    // Pick a starting LOD for your patches (the enumeration still remains on 0, 1, and 2, but the
    // with each starting level, the resulting tiles are subdivided further, resulting in more detailed tiles.
    m_terrain = new PatchedTerrain(2, *this);

    // Task_4_1 - ToDo End

    glClearColor(m_clearColor.x(), m_clearColor.y(), m_clearColor.z(), m_clearColor.w());

    return true;
}


void Painter::keyPressEvent(QKeyEvent * event)
{
    switch (event->key())
    {
    case Qt::Key_L:
        m_drawLineStrips = !m_drawLineStrips;
        update();
        break;
    case Qt::Key_C:
        m_debug = !m_debug;
        update();
        break;
    case Qt::Key_P:
        m_precission *= (event->modifiers() && Qt::Key_Shift ? 1.1f : 1.f / 1.1f);
        patchify();
        break;
    default:
        break;
    }
}


QOpenGLShaderProgram * Painter::createBasicShaderProgram(
    const QString & vertexShaderFileName
    , const QString & fragmentShaderFileName)
{
    QOpenGLShaderProgram * program = new QOpenGLShaderProgram();

    m_shaders << FileAssociatedShader::getOrCreate(
        QOpenGLShader::Vertex, vertexShaderFileName, *program);
    m_shaders << FileAssociatedShader::getOrCreate(
        QOpenGLShader::Fragment, fragmentShaderFileName, *program);
    program->bindAttributeLocation("a_vertex", 0);
    program->link();

    return program;
}

QOpenGLShaderProgram * Painter::createBasicShaderProgram(
    const QString & vertexShaderFileName
    , const QString & geometryShaderFileName
    , const QString & fragmentShaderFileName)
{
    QOpenGLShaderProgram * program = new QOpenGLShaderProgram();

    m_shaders << FileAssociatedShader::getOrCreate(
        QOpenGLShader::Vertex, vertexShaderFileName, *program);
    m_shaders << FileAssociatedShader::getOrCreate(
        QOpenGLShader::Geometry, geometryShaderFileName, *program);
    m_shaders << FileAssociatedShader::getOrCreate(
        QOpenGLShader::Fragment, fragmentShaderFileName, *program);
    program->bindAttributeLocation("a_vertex", 0);
    program->link();

    return program;
}

void Painter::resize(
    int width
    , int height)
{
    glViewport(0, 0, width, height);
    update();
}

void Painter::update()
{
    update(m_programs.values());
}

void Painter::update(const QList<QOpenGLShaderProgram *> & programs)
{
    foreach(const int i, m_programs.keys())
    {
        QOpenGLShaderProgram * program(m_programs[i]);

        if (programs.contains(program) && program->isLinked())
        {
            program->bind();

            switch (i)
            {
            case PaintMode0:
            case PaintMode9:
            case PaintMode8:
            case PaintMode7:
            case PaintMode6:
            case PaintMode5:
            case PaintMode4:
            case PaintMode3:
            case PaintMode2:
            case PaintMode1:

//                if(!m_debug || camera()->eye() != m_cachedEye)
                if(!m_debug)
                {
                    m_cachedEye = camera()->eye();
                    patchify();
                }

                program->setUniformValue("mvp", camera()->viewProjection());
                program->setUniformValue("view", camera()->view());

                program->setUniformValue("yScale",  m_yScale);
                program->setUniformValue("yOffset", m_yOffset);
                program->setUniformValue("clearColor", m_clearColor);

                program->setUniformValue("height",  0);
                program->setUniformValue("normals", 1);
                program->setUniformValue("diffuse", 2);
                program->setUniformValue("detail",  3);
                program->setUniformValue("detailn", 4);
                break;

            //case OtherProgram: // feel free to use more than one program per mode...
            //    break;
            }

            program->release();
        }
    }
}

void Painter::paint(float timef)
{
    switch (mode())
    {
    case PaintMode0:
    case PaintMode1:
        paint_4_1(timef); break;
    case PaintMode2:
    case PaintMode3:
    case PaintMode4:
        //paint_1_4(timef); break;
        //case PaintMode5:
        //    paint_1_5(timef); break;
        // ...
    default:
        break;
    }
}

// returns the height of the terrain at x, z with respect to the vertical
// scale and offset used by the vertex shader and the terrains extend of 8.0

float Painter::height(
    const float x
,   const float z) const
{
    float y = static_cast<float>(m_heights[2048 * qBound<int>(0, 2048 * (z * 0.125 + 0.5f), 2047) + qBound<int>(0, 2048 * (x * 0.125 + 0.5), 2047)]);
    y /= 65536.f;

    return y * m_yScale + m_yOffset;
}

// Note: Feel free to remove your old code and start on minor cleanups and refactorings....

void Painter::patchify()
{
    m_terrain->drawReset();

    // Task_4_1 - ToDo Begin

    // Here you should initiate the "patchification"

    // You can modify the signature of patchify as it pleases you.
    // This function is called whenever the camera changes.

    patchify(8.f, 0.f, 0.f, 0);

    // Task_4_1 - ToDo End
}

bool Painter::cull(
    const QVector4D & v0)
{
    QVector4D v0CSpace = camera()->viewProjection() * v0;
    v0CSpace /= v0CSpace.w();

    //X Axis
    if(v0CSpace.x() < -1.5f || v0CSpace.x() > 1.5)
        return false;

    //X Axis
    if(v0CSpace.y() < -1.5f || v0CSpace.y() > 1.5)
        return false;

    //Z Axis
    if(v0CSpace.z() > camera()->zFar()*1.1 || v0CSpace.z() > 1.0f)
        return false;

    return true;
}

void Painter::drawP(float extend, float x, float z, int level)
{
    QPointF A = QPointF(x - extend/2, z - extend/2);
    QPointF E = QPointF(x - extend/2, z + extend/2);
    QPointF F = QPointF(x + extend/2, z + extend/2);
    QPointF G = QPointF(x + extend/2, z - extend/2);

    QVector<QPointF> points;
    points.append(A);
    points.append(E);
    points.append(F);
    points.append(G);

    bool cullVal = false;

    for(int i = 0; i < points.size(); i++)
    {
        if(cull(QVector4D(points[i].x(), height(points[i].x(), points[i].y()), points[i].y(), 1.f)))
            cullVal = true;
    }

    if(!cullVal)
    {
        m_terrain->drawPatch(QVector3D(x, 0.0, z), extend, 3, 3, 3, 3);
        return;
    }

    int cornerCount = 0;
    float distance;
    float distanceFromLevel = (level - m_maximumDetail)*-1;

    for(int i = 0; i < points.size(); i++)
    {
        distance = (QVector2D(m_cachedEye.x(), m_cachedEye.z()) - QVector2D(points[i].x(), points[i].y())).length();

        if(distance < distanceFromLevel + (std::sqrt(pow(extend, 2)*2.0)))
        {
            cornerCount++;
        }
    }

    //pathces are identified by their number of corners in a specific distance "shell"
    if(cornerCount == 2 )
    {
        int north = 0;
        int east = 0;
        int south = 0;
        int west = 0;
        bool reserved = false;

        float xDistance = m_cachedEye.x() - x;
        float zDistance = m_cachedEye.z() - z;

        if(std::abs(xDistance) > std::abs(zDistance))
        {
                if(xDistance > 0)
                    west = 1;
                else
                    east = 1;
        }
        else
        {
                if(zDistance > 0)
                    south = 1;
                else
                    north = 1;
        }
        m_terrain->drawPatch(QVector3D(x, 0.0, z), extend, north, east, south, west);
        return;
    }
    else if(cornerCount == 3)
    {
        int north = 0;
        int east = 0;
        int south = 0;
        int west = 0;
        bool reserved = false;

        float xDistance = m_cachedEye.x() - x;
        float zDistance = m_cachedEye.z() - z;

        if(xDistance > 0)
            west = 1;
        else
            east = 1;

        if(zDistance > 0)
            south = 1;
        else
            north = 1;

        m_terrain->drawPatch(QVector3D(x, 0.0, z), extend, north, east, south, west);
        return;
    }
    else if(cornerCount == 4)
    {
        m_terrain->drawPatch(QVector3D(x, 0.0, z), extend, 1, 1, 1, 1);
        return;
    }
    else if(cornerCount == 0 || cornerCount == 1)
    {
        m_terrain->drawPatch(QVector3D(x, 0.0, z), extend, 0, 0, 0, 0);
        return;
    }
}

void Painter::patchify(
    const float extend
,   const float x
,   const float z
,   const int level)
{
    //exit condition for maximum detail
    if(level >= m_maximumDetail)
    {
        drawP(extend, x, z, level);
        return;
    }

//                                 x------>
//                                z                <--extend-->
//                                |    A----------------D----------------G
//                                |    |                |                |
//                                |    |                |                |
//                                V    |                |                |
//                                     |       CA       |       CD       |
//                                     |                |                |
//                                   e |                |                |e
//                                   x |                |                |x
//                                   t B----------------C(x|z)-----------It
//                                   e |                |                |e
//                                   n |                |                |n
//                                   d |                |                |d
//                                     |       CB       |       CC       |
//                                     |                |                |
//                                     |                |                |
//                                     |                |                |
//                                     E----------------H----------------F
//                                                 <--extend-->

    QPointF A = QPointF(x - extend/2, z - extend/2);
    QPointF B = QPointF(x - extend/2, z);
    QPointF C = QPointF(x, z);
    QPointF D = QPointF(x, z - extend/2);
    QPointF E = QPointF(x - extend/2, z + extend/2);
    QPointF F = QPointF(x + extend/2, z + extend/2);
    QPointF G = QPointF(x + extend/2, z - extend/2);
    QPointF H = QPointF(x, z + extend/2);
    QPointF I = QPointF(x + extend/2, z);

    QSizeF length = QSizeF(extend, extend);
    QPointF cameraPosition = QPointF(m_cachedEye.x(),  m_cachedEye.z());
    QRectF currentRect = QRectF(A, length);

    bool devide = false;
    QVector<QPointF> points;

    points.append(A);
    points.append(E);
    points.append(F);
    points.append(G);

    for(int i = 0; i < points.size() && !devide; i++)
    {
//        if(level - 1 < levelFromDistance((QVector2D(m_cachedEye.x(), m_cachedEye.z()) - QVector2D(points[i].x(), points[i].y())).length()/extend))
        if(level - 1 < levelFromDistance((m_cachedEye - QVector3D(points[i].x(), 0.f, points[i].y())).length()/extend))
            devide = true;
    }

    if(devide)
    {
        QPointF dist = QPointF(extend/4, extend/4);
        QPointF CA = A + dist;
        QPointF CB = B + dist;
        QPointF CC = C + dist;
        QPointF CD = D + dist;

        patchify(extend/2, CA, level + 1);
        patchify(extend/2, CB, level + 1);
        patchify(extend/2, CC, level + 1);
        patchify(extend/2, CD, level + 1);
    }
    else
    {
        drawP(extend, x, z, level);
    }
}


void Painter::patchify(
    float extend
,   QPointF center
,   int level)
{
    float x = static_cast<float>(center.x());
    float z = static_cast<float>(center.y());
    patchify(extend, x, z, level);
}

int Painter::levelFromDistance(float distance)
{
    if(distance > m_maximumDetail)
        return 0;
    int level;
    if(distance < m_maximumDetail/2)
        level = std::round(m_maximumDetail - distance*0.9);
    else
        level = std::round(m_maximumDetail) - distance;
    return level;
}

void Painter::paint_4_1(float timef)
{
    QOpenGLShaderProgram * program(m_programs[PaintMode1]);

    if (program->isLinked())
    {
        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, m_height);

        glActiveTexture(GL_TEXTURE1);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, m_normals);

        glActiveTexture(GL_TEXTURE2);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, m_diffuse);

        glActiveTexture(GL_TEXTURE3);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, m_detail);

        glActiveTexture(GL_TEXTURE4);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, m_detailn);

        m_terrain->draw(*this, *program, m_drawLineStrips);

        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
