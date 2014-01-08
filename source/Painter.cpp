
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
    m_detail  = FileAssociatedTexture::getOrCreate2D("data/moss_detail.png",   *this, GL_REPEAT, GL_REPEAT);
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

    glClearColor(0.6f, 0.8f, 0.94f, 1.f);

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

                if (!m_debug || camera()->eye() != m_cachedEye)
                {
                    m_cachedEye = camera()->eye();
                    patchify();
                }

                program->setUniformValue("mvp", camera()->viewProjection());
                program->setUniformValue("view", camera()->view());

                program->setUniformValue("yScale",  m_yScale);
                program->setUniformValue("yOffset", m_yOffset);

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

    patchify(8.f, 0.f, 0.f, 0, 0);

    // Task_4_1 - ToDo End
}

bool Painter::cull(
    const QVector4D & v0
,   const QVector4D & v1
,   const QVector4D & v2)
{
    // Task_4_1 - ToDo Begin

    // This function should return true, if the tile specified by vertices v0, v1, and v2
    // is within the cameras view frustum.

    // Hint: you might try to use NDCs and transfer the incomming verticies appropriatelly.
    // With that in mind, it should be simpler to cull...

    // If you like, make use of QVector3D, QVector4D (toVector3DAffine), QPolygonF (boundingRect), and QRectF (intersects)

    // Task_4_1 - ToDo End

    return false;
}

void Painter::patchify(
    const float extend
,   const float x
,   const float z
,   const int level
,   const int corners)
{
    //exit condition for maximum detail
    if(level >= m_maximumDetail)
    {
        m_terrain->drawPatch(QVector3D(x, 0.0, z), extend, 1, 1, 1, 1);
        return;
    }
    if(!m_debug)
    {
        m_cameraPos = camera()->eye();
        m_cameraPos.setY(m_cameraPos.y() - height(m_cameraPos.x(), m_cameraPos.z()));
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
//                                   t B----------------C(x|z)-----------|t
//                                   e |                |                |e
//                                   n |                |                |n
//                                   d |                |                |d
//                                     |       CB       |       CC       |
//                                     |                |                |
//                                     |                |                |
//                                     |                |                |
//                                     E---------------------------------F
//                                                 <--extend-->

    QPointF A =QPointF(x - extend/2, z - extend/2);
    QPointF B =QPointF(x - extend/2, z);
    QPointF C =QPointF(x, z);
    QPointF D =QPointF(x, z - extend/2);
    QPointF E =QPointF(x - extend/2, z + extend/2);
    QPointF F =QPointF(x + extend/2, z + extend/2);
    QPointF G =QPointF(x + extend/2, z - extend/2);

//    qDebug()<<camera()->eye().x()<<" "<<camera()->eye().z()<<" - ("<<x<<" "<<z<<") - ("<<x + extend<<" "<<z + extend<<")";
//    qDebug()<<camera()->eye().y();
//    qDebug()<<levelFromDistance(camera()->eye().y() - height(camera()->eye().x(), camera()->eye().z()));
    QSizeF length = QSizeF(extend, extend);
    QPointF cameraPosition = QPointF(m_cameraPos.x(),  m_cameraPos.z());
    QRectF currentRect = QRectF(A, length);

    float distance = -1;

    QVector<QPointF> points;
    int cornerCount = 0;

    points.append(A);
    points.append(E);
    points.append(F);
    points.append(G);

    for(int i = 0; i < points.size(); i++)
    {
        float tempDistance = (QVector2D(m_cameraPos.x(), m_cameraPos.z()) - QVector2D(points[i].x(), points[i].y())).length()/extend;

        if(level - 1 < levelFromDistance(tempDistance))
        {
            cornerCount++;
            if(distance == -1 || distance < tempDistance)
                distance = tempDistance;
        }
    }

    if(cornerCount > 0)
    {
        QPointF dist = QPointF(extend/4, extend/4);
        QPointF CA = A + dist;
        QPointF CB = B + dist;
        QPointF CC = C + dist;
        QPointF CD = D + dist;

        patchify(extend/2, CA, level + 1, cornerCount);
        patchify(extend/2, CB, level + 1, cornerCount);
        patchify(extend/2, CC, level + 1, cornerCount);
        patchify(extend/2, CD, level + 1, cornerCount);
    }
    else
    {
        int north = 1;
        int east = 1;
        int south = 1;
        int west = 1;
        if((m_cameraPos - QVector3D(E.x(), 0.0f, E.y())).length() < (m_cameraPos - QVector3D(F.x(), 0.0f, F.y())).length())
            west = 0;
        else
            east = 0;

        if((m_cameraPos - QVector3D(A.x(), 0.0f, A.y())).length() < (m_cameraPos - QVector3D(E.x(), 0.0f, E.y())).length())
            south = 0;
        else
            north = 0;

        if(corners == 3)
        {
            m_terrain->drawPatch(QVector3D(x, 0.0, z), extend, north, east, south, west);
        }
        if(corners == 4)
        {
//            m_terrain->drawPatch(QVector3D(x, 0.0, z), extend, 1, 1, 1, 1);
        }
        else if(corners == 1 || corners == 2)
            m_terrain->drawPatch(QVector3D(x, 0.0, z), extend, 0, 0, 0, 0);
    }
    // Use an ad-hoc or "static" approach where you decide to either
    // subdivide the terrain patch and continue with the resulting
    // children (2x2 or 4x4), or initiate a draw call of patch.
    // For the draw call the LODs for all four tiles are required.
    // For skipping a tile (e.g., because of culling), use LOD = 3.

    // This functions signature suggest a recursive approach.
    // Feel free to implement this in any way with as few or as much traversals
    // /passes/recursions you need.

    // Checkt out the paper "Seamless Patches for GPU-Based Terrain Rendering"

    //if () // needs subdivide?
    //{
    //}
    //else // draw patch!
    //{
    //    // check culling

    //    //if (cull(.., .., ..))
    //    //    xLOD = 3;
    //    // ...

//        m_terrain->drawPatch(QVector3D(x, 0.0, z), extend, bLOD, rLOD, tLOD, lLOD);


    //}

    // Task_4_1 - ToDo End
}


void Painter::patchify(
    float extend
,   QPointF center
,   int level
,   const int corners)
{
    float x = static_cast<float>(center.x());
    float z = static_cast<float>(center.y());
    patchify(extend, x, z, level, corners);
}

int Painter::levelFromDistance(float distance)
{
    if(distance > m_maximumDetail)
        return 0;
    int level = std::round(m_maximumDetail - 2*distance);
//    qDebug()<<distance<<" "<<level;
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
