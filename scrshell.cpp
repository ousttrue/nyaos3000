#include "shell.h"

const NnString *ScriptShell::argv(int n) const
{   return (const NnString*)argv_.const_at(n); }

void ScriptShell::shift()
{   delete argv_.shift(); }

int ScriptShell::setLabel( NnString &label )
{
    LabelOnScript *los=new LabelOnScript;
    fr->getpos( los->pos );
    labels.put( label , los );
    return 0;
}
int ScriptShell::goLabel( NnString &label )
{
    LabelOnScript *los=(LabelOnScript*)labels.get(label);
    if( los != NULL ){
        fr->setpos( los->pos );
        return 0;
    }else{
        NnString cmdline;
        fpos_t start;
        fr->getpos( start );
        for(;;){
            int rc=this->readcommand(cmdline);
            if( rc < 0 ){
                fr->setpos( start );
                return 0;
            }
            cmdline.trim();
            if( cmdline.at(0) == ':' ){
                NnString label2( cmdline.chars()+1 );
                label2.trim();
                if( label2.length() > 0 ){
                    setLabel( label2 );
                    if( label2.compare(label)==0 )
                        return 0;
                }
            }
        }
    }
}

int ScriptShell::addArgv( const NnString &arg )
{
    NnObject *element = arg.clone();
    if( element == NULL )
        return -1;
    return argv_.append( element );
}
