// dragsource FILE...: a window (at 1300,80) that starts an X drag (XDND)
// of FILE... as text/uri-list when it is pressed, as a file manager does.
// For kwhatsapp's drop test: xdotool presses here, moves, releases there.
#include <qapplication.h>
#include <qdragobject.h>
#include <qlabel.h>
#include <qstrlist.h>

class Source : public QLabel
{
public:
    Source() : QLabel( "DRAG FILES", 0, "dragsource" ) { setAlignment( AlignCenter ); }
    QStrList uris;

protected:
    void mousePressEvent( QMouseEvent* )
    {
        QUriDrag* drag = new QUriDrag( uris, this );
        drag->drag();
    }
};

int main( int argc, char** argv )
{
    QApplication app( argc, argv );
    Source source;
    for ( int i = 1; i < argc; ++i )
        source.uris.append( QUriDrag::localFileToUri( QString::fromLocal8Bit( argv[i] ) ) );
    source.setCaption( "dragsource" );
    source.setGeometry( 1300, 80, 200, 200 );
    source.show();
    app.setMainWidget( &source );
    return app.exec();
}
