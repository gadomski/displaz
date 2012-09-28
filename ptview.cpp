// Copyright (C) 2001, Paul C. Gregory and the other authors and contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the software's owners nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// (This is the New BSD license)

#include <GL/glew.h>

#include "ptview.h"

#define GL_GLEXT_PROTOTYPES

#include <QtCore/QSignalMapper>
#include <QtGui/QApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QMenuBar>
#include <QtGui/QMessageBox>
#include <QtGui/QFileDialog>
#include <QtGui/QColorDialog>

#ifdef _WIN32
#   define NOMINMAX
#   include <ImathVec.h>
#   include <ImathMatrix.h>
#else
#   include <OpenEXR/ImathVec.h>
#   include <OpenEXR/ImathMatrix.h>
#endif

#include "ptview.h"
#include "argparse.h"

#include <lasreader.hpp>

//----------------------------------------------------------------------
//#include <OpenEXR/ImathGL.h>
// Utilities for OpenEXR / OpenGL interoperability.
//
// Technically we could use the stuff from ImathGL instead here, but it has
// portability problems for OSX due to how it includes gl.h (this is an
// libilmbase bug, at least up until 1.0.2)
inline void glTranslate(const Imath::V3f& v)
{
    glTranslatef(v.x, v.y, v.z);
}

inline void glVertex(const Imath::V3f& v)
{
    glVertex3f(v.x, v.y, v.z);
}

inline void glVertex(const Imath::V2f& v)
{
    glVertex2f(v.x, v.y);
}

inline void glColor(const Imath::C3f& c)
{
    glColor3f(c.x, c.y, c.z);
}

inline void glLoadMatrix(const Imath::M44f& m)
{
    glLoadMatrixf((GLfloat*)m[0]);
}


//----------------------------------------------------------------------
inline float rad2deg(float r)
{
    return r*180/M_PI;
}

inline QVector3D exr2qt(const Imath::V3f& v)
{
    return QVector3D(v.x, v.y, v.z);
}

inline Imath::V3f qt2exr(const QVector3D& v)
{
    return Imath::V3f(v.x(), v.y(), v.z());
}

inline Imath::M44f qt2exr(const QMatrix4x4& m)
{
    Imath::M44f mOut;
    for(int j = 0; j < 4; ++j)
    for(int i = 0; i < 4; ++i)
        mOut[j][i] = m.constData()[4*j + i];
    return mOut;
}


//----------------------------------------------------------------------
// PointViewArray implementation

PointArrayModel::PointArrayModel()
    : m_npoints(0)
{ }


bool PointArrayModel::loadPointFile(const QString& fileName, size_t maxPointCount,
                                    const C3f& color)
{
    LASreadOpener lasReadOpener;
    lasReadOpener.set_file_name(fileName.toAscii().constData());
    std::unique_ptr<LASreader> lasReader(lasReadOpener.open());

    if(!lasReader)
    {
        QMessageBox::critical(0, tr("Error"),
            tr("Couldn't open file \"%1\"").arg(fileName));
        return false;
    }

    // Figure out how much to decimate the point cloud.
    size_t totPoints = lasReader->header.number_of_point_records;
    size_t decimate = (totPoints + maxPointCount - 1) / maxPointCount;
    if(decimate > 1)
    {
        std::cout << "Decimating \"" << fileName.toStdString() << "\" by factor of " << decimate << std::endl;
    }
    m_npoints = (totPoints + decimate - 1) / decimate;
    Imath::V3d offset = Imath::V3d(lasReader->header.min_x,
                                   lasReader->header.min_y,
                                   lasReader->header.min_z);
    m_P.reset(new V3f[m_npoints]);
    // TODO: Look for color channel?
    m_color.reset(new C3f[m_npoints]);
    // Iterate over all particles & pull in the data.
    V3f* outP = m_P.get();
    V3f* outCol = m_color.get();
    size_t readCount = 0;
    size_t nextBlock = 1;
    size_t nextStore = 1;
    while(lasReader->read_point())
    {
        // Read a point from the las file
        const LASpoint& point = lasReader->point;
        ++readCount;
        if(readCount % 10000 == 0)
            emit loadedPoints(double(readCount)/totPoints);
        if(readCount < nextStore)
            continue;
        // Store the point
        *outP++ = Imath::V3d(point.get_x(), point.get_y(), point.get_z()) - offset;
        // float intens = float(point.scan_angle_rank) / 40;
        float intens = float(point.intensity) / 400;
        intens = intens / (1 + intens);
        //float intens = 0.5*point.point_source_ID;
        *outCol++ = color*intens;
        // Figure out which point will be the next stored point.
        nextBlock += decimate;
        nextStore = nextBlock;
        if(decimate > 1)
        {
            // Randomize selected point within block to avoid repeated patterns
            nextStore += (qrand() % decimate);
            if(nextBlock <= totPoints && nextStore > totPoints)
                nextStore = totPoints;
        }
    }
    lasReader->close();
    std::cout << "Read " << totPoints << " points.  Displaying " << m_npoints <<  std::endl;
    return true;
}


V3f PointArrayModel::centroid() const
{
    V3f sum(0);
    const V3f* P = m_P.get();
    for(size_t i = 0; i < m_npoints; ++i, ++P)
        sum += *P;
    return (1.0f/m_npoints) * sum;
}


//------------------------------------------------------------------------------
PointView::PointView(QWidget *parent)
    : QGLWidget(parent),
    m_camera(false),
    m_lastPos(0,0),
    m_zooming(false),
    m_cursorPos(0),
    m_backgroundColor(60, 50, 50),
    m_drawAxes(false),
    m_points(),
    m_cloudCenter(0),
    m_maxPointCount(10000000)
{
    setFocusPolicy(Qt::StrongFocus);

    m_camera.setClipFar(FLT_MAX);
    connect(&m_camera, SIGNAL(projectionChanged()), this, SLOT(updateGL()));
    connect(&m_camera, SIGNAL(viewChanged()), this, SLOT(updateGL()));
}


void PointView::loadPointFiles(const QStringList& fileNames)
{
    size_t maxCount = m_maxPointCount / fileNames.size();
    m_points.clear();
    C3f colors[] = {C3f(1,1,1), C3f(1,0.5,0.5), C3f(0.5,1,0.5), C3f(0.5,0.5,1)};
    for(int i = 0; i < fileNames.size(); ++i)
    {
        std::unique_ptr<PointArrayModel> points(new PointArrayModel());
        if(points->loadPointFile(fileNames[i], maxCount,
                                 colors[i%(sizeof(colors)/sizeof(C3f))]) &&
           !points->empty())
            m_points.push_back(std::move(points));
    }
    if(m_points.empty())
        return;
    emit colorChannelsChanged(m_points[0]->colorChannels());
    m_cloudCenter = m_points[0]->centroid();
    m_cursorPos = m_cloudCenter;
    m_camera.setCenter(exr2qt(m_cloudCenter));
    updateGL();
}


void PointView::setBackground(QColor col)
{
    m_backgroundColor = col;
    updateGL();
}


void PointView::toggleDrawAxes()
{
    m_drawAxes = !m_drawAxes;
}


void PointView::setColorChannel(QString channel)
{
    for(size_t i = 0; i < m_points.size(); ++i)
        m_points[i]->setColorChannel(channel);
    updateGL();
}


void PointView::setMaxPointCount(size_t maxPointCount)
{
    m_maxPointCount = maxPointCount;
}


QSize PointView::sizeHint() const
{
    // Size hint, mainly for getting the initial window size right.
    // setMinimumSize() also sort of works for this, but doesn't allow the
    // user to later make the window smaller.
    return QSize(640,480);
}


void PointView::initializeGL()
{
    //glEnable(GL_MULTISAMPLE);
    if(glewInit() != GLEW_OK)
        close();
}


void PointView::resizeGL(int w, int h)
{
    // Draw on full window
    glViewport(0, 0, w, h);
    m_camera.setViewport(geometry());
}


void PointView::paintGL()
{
    //--------------------------------------------------
    // Draw main scene
    // Set camera projection
    glMatrixMode(GL_PROJECTION);
    glLoadMatrix(qt2exr(m_camera.projectionMatrix()));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrix(qt2exr(m_camera.viewMatrix()));

    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearColor(m_backgroundColor.redF(), m_backgroundColor.greenF(),
                 m_backgroundColor.blueF(), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw geometry
    if(m_drawAxes)
        drawAxes();
    for(size_t i = 0; i < m_points.size(); ++i)
        drawPoints(*m_points[i]);

    // Draw overlay stuff, including cursor position.
    drawCursor(m_cursorPos);
}


void PointView::mousePressEvent(QMouseEvent* event)
{
    m_zooming = event->button() == Qt::RightButton;
    m_lastPos = event->pos();
}


void PointView::mouseMoveEvent(QMouseEvent* event)
{
    if(event->modifiers() & Qt::ControlModifier)
    {
        m_cursorPos = qt2exr(
            m_camera.mouseMovePoint(exr2qt(m_cursorPos),
                                    event->pos() - m_lastPos,
                                    m_zooming) );
        updateGL();
    }
    else
        m_camera.mouseDrag(m_lastPos, event->pos(), m_zooming);
    m_lastPos = event->pos();
}


void PointView::wheelEvent(QWheelEvent* event)
{
    // Translate mouse wheel events into vertical dragging for simplicity.
    m_camera.mouseDrag(QPoint(0,0), QPoint(0, -event->delta()/2), true);
}


void PointView::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_C)
    {
        m_camera.setCenter(exr2qt(m_cursorPos));
    }
    else if(event->key() == Qt::Key_S)
    {
        // Snap probe to position of closest point and center on it
        V3f newPos(0);
        size_t nearestIdx = 0;
        float nearestDist = FLT_MAX;
        for(size_t j = 0; j < m_points.size(); ++j)
        {
            if(m_points[j]->empty())
                continue;
            const V3f* P = m_points[j]->P();
            for(size_t i = 0, iend = m_points[j]->size(); i < iend; ++i, ++P)
            {
                float dist = (m_cursorPos - *P).length2();
                if(dist < nearestDist)
                {
                    nearestDist = dist;
                    newPos = *P;
                    nearestIdx = i;
                }
            }
        }
        std::cout << "Selected index " << nearestIdx << "\n";
        m_cursorPos = newPos;
        m_camera.setCenter(exr2qt(newPos));
        updateGL();
    }
    else
        event->ignore();
}


/// Draw a set of axes
void PointView::drawAxes()
{
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(1);
    glColor3f(1.0, 0.0, 0.0);
    glBegin(GL_LINES);
        glVertex3f(0, 0, 0);
        glVertex3f(1, 0, 0);
    glEnd();
    glColor3f(0.0, 1.0, 0.0);
    glBegin(GL_LINES);
        glVertex3f(0, 0, 0);
        glVertex3f(0, 1, 0);
    glEnd();
    glColor3f(0.0, 0.0, 1.0);
    glBegin(GL_LINES);
        glVertex3f(0, 0, 0);
        glVertex3f(0, 0, 1);
    glEnd();
}


/// Draw the 3D cursor
void PointView::drawCursor(const V3f& p) const
{
    // Draw a point at the centre of the cursor.
    glColor3f(1,1,1);
    glPointSize(10);
    glBegin(GL_POINTS);
        glVertex(m_cursorPos);
    glEnd();

    // Find position of cursor in screen space
    V3f screenP3 = qt2exr(m_camera.projectionMatrix()*m_camera.viewMatrix() *
                          exr2qt(m_cursorPos));
    if(screenP3.z < 0)
        return; // Cull if behind the camera.  TODO: doesn't work quite right

    // Now draw a 2D overlay over the 3D scene to allow user to pinpoint the
    // cursor, even when when it's behind something.
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0,width(), 0,height(), 0, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);

    // Position in ortho coord system
    V2f p2 = 0.5f * V2f(width(), height()) *
             (V2f(screenP3.x, screenP3.y) + V2f(1.0f));
    float r = 10;
    glLineWidth(2);
    glColor3f(1,1,1);
    // Combined white and black crosshairs, so they can be seen on any
    // background.
    glTranslatef(p2.x, p2.y, 0);
    glBegin(GL_LINES);
        glVertex(V2f(r,   0));
        glVertex(V2f(2*r, 0));
        glVertex(-V2f(r,   0));
        glVertex(-V2f(2*r, 0));
        glVertex(V2f(0,   r));
        glVertex(V2f(0, 2*r));
        glVertex(-V2f(0,   r));
        glVertex(-V2f(0, 2*r));
    glEnd();
    glColor3f(0,0,0);
    glRotatef(45,0,0,1);
    glBegin(GL_LINES);
        glVertex(V2f(r,   0));
        glVertex(V2f(2*r, 0));
        glVertex(-V2f(r,   0));
        glVertex(-V2f(2*r, 0));
        glVertex(V2f(0,   r));
        glVertex(V2f(0, 2*r));
        glVertex(-V2f(0,   r));
        glVertex(-V2f(0, 2*r));
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}


/// Draw point cloud using OpenGL
void PointView::drawPoints(const PointArrayModel& points)
{
    if(points.empty())
        return;
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_LIGHTING);
    // Draw points
    glPointSize(5);
    glColor3f(1,1,1);
    // Set distance attenuation for points, following the usual 1/z
    // law.
    GLfloat attenParams[3] = {0, 0, 0.001f};
    glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, attenParams);
    glPointParameterf(GL_POINT_SIZE_MIN, 0);
    glPointParameterf(GL_POINT_SIZE_MAX, 100);
    // Draw all points at once using vertex arrays.
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    size_t chunkSize = 1000000;
    for (size_t i = 0; i < points.size(); i += chunkSize)
    {
        const float* P = reinterpret_cast<const float*>(points.P() + i);
        const float* col = reinterpret_cast<const float*>(points.color() + i);
        glVertexPointer(3, GL_FLOAT, 3*sizeof(float), P);
        glColorPointer(3, GL_FLOAT, 3*sizeof(float), col);
        int ndraw = (int)std::min(points.size() - i, chunkSize);
        glDrawArrays(GL_POINTS, 0, ndraw);
    }
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}


//------------------------------------------------------------------------------
// PointViewerMainWindow implementation

PointViewerMainWindow::PointViewerMainWindow(
        const QStringList& initialPointFileNames)
    : m_pointView(0),
    m_colorMenu(0),
    m_colorMenuGroup(0),
    m_colorMenuMapper(0)
{
    setWindowTitle("qtlasview");

    // File menu
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* openAct = fileMenu->addAction(tr("&Open"));
    openAct->setStatusTip(tr("Open a point cloud file"));
    openAct->setShortcuts(QKeySequence::Open);
    connect(openAct, SIGNAL(triggered()), this, SLOT(openFiles()));
    QAction* quitAct = fileMenu->addAction(tr("&Quit"));
    quitAct->setStatusTip(tr("Exit the application"));
    quitAct->setShortcuts(QKeySequence::Quit);
    connect(quitAct, SIGNAL(triggered()), this, SLOT(close()));

    // View menu
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    QAction* drawAxes = viewMenu->addAction(tr("Draw &Axes"));
    drawAxes->setCheckable(true);
    // Background sub-menu
    QMenu* backMenu = viewMenu->addMenu(tr("Set &Background"));
    QSignalMapper* mapper = new QSignalMapper(this);
    // Selectable backgrounds (svg_names from SVG standard - see QColor docs)
    const char* backgroundNames[] = {/* "Display Name", "svg_name", */
                                        "&Black",        "black",
                                        "&Dark Grey",    "dimgrey",
                                        "&Light Grey",   "lightgrey",
                                        "&White",        "white" };
    for(size_t i = 0; i < sizeof(backgroundNames)/sizeof(const char*); i+=2)
    {
        QAction* backgroundAct = backMenu->addAction(tr(backgroundNames[i]));
        mapper->setMapping(backgroundAct, backgroundNames[i+1]);
        connect(backgroundAct, SIGNAL(triggered()), mapper, SLOT(map()));
    }
    connect(mapper, SIGNAL(mapped(QString)),
            this, SLOT(setBackground(QString)));
    backMenu->addSeparator();
    QAction* backgroundCustom = backMenu->addAction(tr("&Custom"));
    connect(backgroundCustom, SIGNAL(triggered()),
            this, SLOT(chooseBackground()));
    // Color channel menu
    m_colorMenu = viewMenu->addMenu(tr("Color &Channel"));
    m_colorMenuMapper = new QSignalMapper(this);

    // Help menu
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* helpAct = helpMenu->addAction(tr("&Controls"));
    connect(helpAct, SIGNAL(triggered()), this, SLOT(helpDialog()));
    helpMenu->addSeparator();
    QAction* aboutAct = helpMenu->addAction(tr("&About"));
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(aboutDialog()));

    m_pointView = new PointView(this);
    connect(m_pointView, SIGNAL(colorChannelsChanged(QStringList)),
            this, SLOT(setColorChannels(QStringList)));
    connect(m_colorMenuMapper, SIGNAL(mapped(QString)),
            m_pointView, SLOT(setColorChannel(QString)));
    connect(drawAxes, SIGNAL(triggered()),
            m_pointView, SLOT(toggleDrawAxes()));

    setCentralWidget(m_pointView);
    if(!initialPointFileNames.empty())
        m_pointView->loadPointFiles(initialPointFileNames);
}


void PointViewerMainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Escape)
        close();
}


void PointViewerMainWindow::openFiles()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
            tr("Select one or more point clouds to open"), "",
            tr("Point cloud files (*.las *.laz)"));
    if(!fileNames.empty())
        m_pointView->loadPointFiles(fileNames);
}


void PointViewerMainWindow::helpDialog()
{
    QString message = tr(
        "ptview controls:\n"
        "\n"
        "LMB+drag = rotate camera\n"
        "RMB+drag = zoom camera\n"
        "Ctrl+LMB+drag = move 3D cursor\n"
        "Ctrl+RMB+drag = zoom 3D cursor along view direction\n"
        "'c' = center camera on 3D cursor\n"
        "'v' = toggle view mode between points and disks\n"
        "'s' = snap 3D cursor to nearest point\n"
        "\n"
        "(LMB, RMB = left & right mouse buttons)\n"
    );
    QMessageBox::information(this, tr("ptview control summary"), message);
}


void PointViewerMainWindow::aboutDialog()
{
    QString message = tr("Qt las viewer\nversion 0.0.1");
    QMessageBox::information(this, tr("About ptview"), message);
}


void PointViewerMainWindow::setBackground(const QString& name)
{
    m_pointView->setBackground(QColor(name));
}


void PointViewerMainWindow::chooseBackground()
{
    m_pointView->setBackground(
        QColorDialog::getColor(QColor(255,255,255), this, "background color"));
}


void PointViewerMainWindow::setColorChannels(QStringList channels)
{
    // Remove the old set of color channels from the menu
    delete m_colorMenuGroup;
    m_colorMenuGroup = new QActionGroup(this);
    m_colorMenuGroup->setExclusive(true);
    if(channels.empty())
        return;
    // Rebuild the color channel menu with a menu item for each channel
    for(int i = 0; i < channels.size(); ++i)
    {
        QAction* act = m_colorMenuGroup->addAction(channels[i]);
        act->setCheckable(true);
        m_colorMenu->addAction(act);
        m_colorMenuMapper->setMapping(act, channels[i]);
        connect(act, SIGNAL(triggered()), m_colorMenuMapper, SLOT(map()));
    }
    m_colorMenuGroup->actions()[0]->setChecked(true);
}


//------------------------------------------------------------------------------


QStringList g_pointFileNames;
static int storeFileName (int argc, const char *argv[])
{
    for(int i = 0; i < argc; ++i)
        g_pointFileNames.push_back (argv[i]);
    return 0;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    ArgParse::ArgParse ap;
    int maxPointCount = 10000000;

    ap.options(
        "qtlasview - view a LAS point cloud\n"
        "Usage: qtlasview [opts] file1.las [file2.las ...]",
        "%*", storeFileName, "",
        "-maxpoints %d", &maxPointCount, "Maximum number of points to load at a time",
        NULL
    );

    if(ap.parse(argc, const_cast<const char**>(argv)) < 0)
    {
        std::cerr << ap.geterror() << std::endl;
        ap.usage();
        return EXIT_FAILURE;
    }

    // Turn on multisampled antialiasing - this makes rendered point clouds
    // look much nicer.
    QGLFormat f = QGLFormat::defaultFormat();
    //f.setSampleBuffers(true);
    QGLFormat::setDefaultFormat(f);

    PointViewerMainWindow window;
    window.pointView().setMaxPointCount(maxPointCount);
    if(!g_pointFileNames.empty())
        window.pointView().loadPointFiles(g_pointFileNames);
    window.show();

    return app.exec();
}


// vi: set et:
