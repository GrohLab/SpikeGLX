
#include "Util.h"
#include "MainApp.h"
#include "Run.h"
#include "GraphFetcher.h"
#include "GraphsWindow.h"
#include "RunToolbar.h"
#include "GWSelectWidget.h"
#include "GWLEDWidget.h"
#include "SView.h"
#include "SVGrafsM.h"
#include "ColorTTLCtl.h"
#include "ConfigCtl.h"

#include <QSplitter>
//#include <QVBoxLayout>
#include <QKeyEvent>
#include <QStatusBar>
#include <QMessageBox>
#include <QSettings>


/* ---------------------------------------------------------------- */
/* Globals -------------------------------------------------------- */
/* ---------------------------------------------------------------- */

const QColor    NeuGraphBGColor( 0x20, 0x3c, 0x3c, 0xff ),
                LfpGraphBGColor( 0x55, 0x55, 0x7f, 0xff ),
                AuxGraphBGColor( 0x4f, 0x4f, 0x4f, 0xff ),
                DigGraphBGColor( 0x1f, 0x1f, 0x1f, 0xff );

/* ---------------------------------------------------------------- */
/* class GraphsWindow --------------------------------------------- */
/* ---------------------------------------------------------------- */

// Solution for invisible grab handle thanks to:
// David Walthall--
// <http://stackoverflow.com/questions/2545577/
// qsplitter-becoming-undistinguishable-between-qwidget-and-qtabwidget>
//
#if 0
static void visibleGrabHandle( QSplitter *sp )
{
    QSplitterHandle *handle = sp->handle( 1 );
    QVBoxLayout     *layout = new QVBoxLayout( handle );
    layout->setSpacing( 0 );
    layout->setMargin( 0 );

    QFrame  *line = new QFrame( handle );
    line->setFrameShape( QFrame::HLine );
    line->setFrameShadow( QFrame::Sunken );
    layout->addWidget( line );
}
#endif


GraphsWindow::GraphsWindow( const DAQ::Params &p )
    :   QMainWindow(0), p(p), lW(0), rW(0), TTLCC(0)
{
// Install widgets

    addToolBar( tbar = new RunToolbar( this, p ) );

    statusBar()->
        addPermanentWidget( LED = new GWLEDWidget( p ) );
    statusBar()->
        insertPermanentWidget( 0, SEL = new GWSelectWidget( this, p ) );

    QSplitter   *sp = new QSplitter;
    sp->setOrientation( Qt::Horizontal );   // streams left to right

#if 0
    visibleGrabHandle( sp );
#endif

    setCentralWidget( sp );

// Screen state

    restoreScreenState();

// Other helpers

    TTLCC = new ColorTTLCtl( this, p );
}


// Note:
// Destructor order: {GW, toolbar, LED, niW}.
//
GraphsWindow::~GraphsWindow()
{
    saveScreenState();
    setUpdatesEnabled( false );
}


void GraphsWindow::eraseGraphs()
{
    if( lW )
        lW->eraseGraphs();

    if( rW )
        rW->eraseGraphs();
}

/* ---------------------------------------------------------------- */
/* Slots ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

void GraphsWindow::initViews()
{
    Run     *run        = mainApp()->getRun();
    bool    wasPaused   = run->grfHardPause( true );

    QSplitter   *sp = dynamic_cast<QSplitter*>(centralWidget());

    setUpdatesEnabled( false );

    installLeft( sp );

    if( installRight( sp ) ) {

        sp->refresh();

        QList<int>  sz  = sp->sizes();
        int         sm  = sz[0] + sz[1],
                    av  = sm/2;

        sz[0] = av;
        sz[1] = sm - av;

        sp->setSizes( sz );
    }

    SEL->updateSelections();
    setUpdatesEnabled( true );
    setCursor( Qt::ArrowCursor );

// -------
// Helpers
// -------

    initColorTTL();
    initGFStreams();

    if( !wasPaused )
        run->grfHardPause( false );
}


void GraphsWindow::updateRHSFlags()
{
    if( lW )
        lW->updateRHSFlags();

    if( rW )
        rW->updateRHSFlags();
}


bool GraphsWindow::remoteIsUsrOrderIm()
{
    if( lW && SEL->lType() >= 0 )
        return lW->isUsrOrder();
    else if( rW && SEL->rType() >= 0 )
        return rW->isUsrOrder();

    return false;
}


bool GraphsWindow::remoteIsUsrOrderNi()
{
    if( lW && SEL->lType() == -1 )
        return lW->isUsrOrder();
    else if( rW && SEL->rType() == -1 )
        return rW->isUsrOrder();

    return false;
}


void GraphsWindow::remoteSetRecordingEnabled( bool on )
{
    tbar->setRecordingEnabled( on );
    tbSetRecordingEnabled( on );
}


void GraphsWindow::remoteSetRunLE( const QString &name )
{
    tbar->setRunLE( name );
}


void GraphsWindow::setGateLED( bool on )
{
    LED->setGateLED( on );
}


void GraphsWindow::setTriggerLED( bool on )
{
    LED->setTriggerLED( on );
}


void GraphsWindow::blinkTrigger()
{
    LED->blinkTrigger();
}


void GraphsWindow::updateOnTime( const QString &s )
{
    tbar->updateOnTime( s );
}


void GraphsWindow::updateRecTime( const QString &s )
{
    tbar->updateRecTime( s );
}


void GraphsWindow::updateGT( const QString &s )
{
    tbar->updateGT( s );
}


void GraphsWindow::tbSetRecordingEnabled( bool checked )
{
    ConfigCtl*  cfg = mainApp()->cfgCtl();
    Run*        run = mainApp()->getRun();

    if( checked ) {

        QRegExp re("(.*)_[gG](\\d+)_[tT](\\d+)$");
        QString name    = tbar->getRunLE();
        int     g       = -1,
                t       = -1;

        if( name.contains( re ) ) {

            name    = re.cap(1);
            g       = re.cap(2).toInt();
            t       = re.cap(3).toInt();
        }

        if( name.compare( p.sns.runName, Qt::CaseInsensitive ) != 0 ) {

            // different run name...reset gt counters

            QString err;

            if( !cfg->validRunName( err, name, this, true ) ) {

                if( !err.isEmpty() )
                    QMessageBox::warning( this, "Run Name Error", err );

                tbar->setRecordingEnabled( false );
                return;
            }

            cfg->externSetsRunName( name );
            run->grfUpdateWindowTitles();
            run->dfResetGTCounters();
        }
        else if( t > -1 ) {

            // Same run name...adopt given gt counters;
            // then obliterate user indices so on a
            // subsequent pause they don't get read again.

            run->dfForceGTCounters( g, t );
            tbar->setRunLE( name );
        }

        LED->setOnColor( QLED::Green );
    }
    else
        LED->setOnColor( QLED::Yellow );

    if( lW )
        lW->setRecordingEnabled( checked );

    if( rW )
        rW->setRecordingEnabled( checked );

    tbar->enableRunLE( !checked );
    run->dfSetRecordingEnabled( checked );
}

/* ---------------------------------------------------------------- */
/* Protected ------------------------------------------------------ */
/* ---------------------------------------------------------------- */

// Force Ctrl+A events to be treated as 'show audio dialog',
// instead of 'text-field select-all'.
//
bool GraphsWindow::eventFilter( QObject *watched, QEvent *event )
{
    if( event->type() == QEvent::KeyPress ) {

        QKeyEvent   *e = static_cast<QKeyEvent*>(event);

        if( e->modifiers() == Qt::ControlModifier ) {

            if( e->key() == Qt::Key_A ) {
                mainApp()->act.aoDlgAct->trigger();
                e->ignore();
                return true;
            }
        }
    }

    return QMainWindow::eventFilter( watched, event );
}


void GraphsWindow::keyPressEvent( QKeyEvent *e )
{
    if( e->modifiers() == Qt::ControlModifier ) {

        if( e->key() == Qt::Key_L ) {

            mainApp()->act.shwHidConsAct->trigger();
            e->accept();
            return;
        }
    }
    else if( e->key() == Qt::Key_Escape ) {

        close();
        e->accept();
        return;
    }

    QWidget::keyPressEvent( e );
}


// Detect show: pause if hidden.
//
void GraphsWindow::showEvent( QShowEvent *e )
{
    QWidget::showEvent( e );

    QMetaObject::invokeMethod(
        mainApp()->getRun(),
        "grfSoftPause",
        Qt::QueuedConnection,
        Q_ARG(bool, false) );
}


// Detect hide: pause if hidden.
//
void GraphsWindow::hideEvent( QHideEvent *e )
{
    QWidget::hideEvent( e );

    QMetaObject::invokeMethod(
        mainApp()->getRun(),
        "grfSoftPause",
        Qt::QueuedConnection,
        Q_ARG(bool, true) );
}


// Detect window minimized: pause graphing if so.
//
void GraphsWindow::changeEvent( QEvent *e )
{
    if( e->type() == QEvent::WindowStateChange ) {

        QWindowStateChangeEvent *wsce =
            static_cast<QWindowStateChangeEvent*>( e );

        if( wsce->oldState() & Qt::WindowMinimized ) {

            if( !(windowState() & Qt::WindowMinimized) ) {

                QMetaObject::invokeMethod(
                    mainApp()->getRun(),
                    "grfSoftPause",
                    Qt::QueuedConnection,
                    Q_ARG(bool, false) );
            }
        }
        else {
            if( windowState() & Qt::WindowMinimized ) {

                QMetaObject::invokeMethod(
                    mainApp()->getRun(),
                    "grfSoftPause",
                    Qt::QueuedConnection,
                    Q_ARG(bool, true) );
            }
        }
    }

    QWidget::changeEvent( e );
}


// Intercept the close box. Rather than close here,
// we ask the run manager if it's OK to close and if
// so, the run manager will delete us as part of the
// stopTask sequence.
//
void GraphsWindow::closeEvent( QCloseEvent *e )
{
    e->ignore();

    QMetaObject::invokeMethod(
        mainApp(), "file_AskStopRun",
        Qt::QueuedConnection );
}

/* ---------------------------------------------------------------- */
/* Private -------------------------------------------------------- */
/* ---------------------------------------------------------------- */

void GraphsWindow::installLeft( QSplitter *sp )
{
    if( !SEL->lChanged() )
        return;

    int type = SEL->lType();

    if( sp->count() > 0 ) {

        QWidget     *w;
        QByteArray  geom;
        bool        shks = lW->shankCtlState( geom );

        if( type >= 0 )
            w = sp->replaceWidget( 0, new SViewM_Im( lW, this, p, type ) );
        else
            w = sp->replaceWidget( 0, new SViewM_Ni( lW, this, p ) );

        if( w )
            delete w;

        if( shks ) {

            lW->shankCtlRestore( geom );

            QMetaObject::invokeMethod(
                SEL, "setFocus",
                Qt::QueuedConnection,
                Q_ARG(bool, false) );
        }
    }
    else {

        if( type >= 0 )
            sp->addWidget( new SViewM_Im( lW, this, p, type ) );
        else
            sp->addWidget( new SViewM_Ni( lW, this, p ) );
    }
}


bool GraphsWindow::installRight( QSplitter *sp )
{
// ----------------
// Already have two
// ----------------

    if( sp->count() == 2 ) {

        if( SEL->rChecked() ) {

            if( SEL->rChanged() ) {

                QWidget     *w;
                QByteArray  geom;
                int         type = SEL->rType();
                bool        shks = rW->shankCtlState( geom );

                if( type >= 0 )
                    w = sp->replaceWidget( 1, new SViewM_Im( rW, this, p, type ) );
                else
                    w = sp->replaceWidget( 1, new SViewM_Ni( rW, this, p ) );

                if( w )
                    delete w;

                if( shks ) {

                    rW->shankCtlRestore( geom );

                    QMetaObject::invokeMethod(
                        SEL, "setFocus",
                        Qt::QueuedConnection,
                        Q_ARG(bool, true) );
                }
            }
            else
                return false;
        }
        else {
            delete sp->widget( 1 );
            rW = 0;
        }

        return false;
    }

// -----------
// Adding one?
// -----------

    if( SEL->rChecked() ) {

        int type = SEL->rType();

        if( type >= 0 )
            sp->addWidget( new SViewM_Im( rW, this, p, type ) );
        else
            sp->addWidget( new SViewM_Ni( rW, this, p ) );

        return true;
    }

// ----------
// Not adding
// ----------

    return false;
}


void GraphsWindow::initColorTTL()
{
    const AIQ   *Qa = 0, *Qb = 0;
    MGraphX     *Xa = 0, *Xb = 0;
    Run         *run    = mainApp()->getRun();
    int         lType   = -2,
                rType   = -2;

    if( lW ) {
        lType = SEL->lType();
        Qa = (lType >= 0 ? run->getImQ( lType ) : run->getNiQ());
        Xa = lW->getTheX();
    }

    if( rW ) {
        rType = SEL->rType();
        Qb = (rType >= 0 ? run->getImQ( rType ) : run->getNiQ());
        Xb = rW->getTheX();
    }

    TTLCC->setClients( lType, Qa, Xa, rType, Qb, Xb );
}


void GraphsWindow::initGFStreams()
{
    QVector<GFStream>   gfs;

    if( lW )
        gfs.push_back( GFStream( SEL->lStream(), lW ) );

    if( rW )
        gfs.push_back( GFStream( SEL->rStream(), rW ) );

    mainApp()->getRun()->grfSetStreams( gfs );
}


void GraphsWindow::saveScreenState()
{
    STDSETTINGS( settings, "windowlayout" );

    settings.setValue( "WinLayout_Graphs/geometry", saveGeometry() );
    settings.setValue( "WinLayout_Graphs/windowState", saveState() );
}


void GraphsWindow::restoreScreenState()
{
    STDSETTINGS( settings, "windowlayout" );

    if( !restoreGeometry(
        settings.value( "WinLayout_Graphs/geometry" ).toByteArray() )
        ||
        !restoreState(
        settings.value( "WinLayout_Graphs/windowState" ).toByteArray() ) ) {

        resize( 1280, 768 );
    }
}


