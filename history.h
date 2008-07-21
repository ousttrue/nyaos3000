#ifndef HISTORY_H
#define HISTORY_H

class Reader;

class History {
    NnVector history;
public:
    History &operator << ( const NnString & );
    NnString *operator[](int at);
    NnString *top(){
	return history.size() >= 1 ? (NnString*)history.top() : NULL ; 
    }
    void drop(){ delete (NnString*)history.pop(); }
    int size();
    void pack();
    int set(int at,NnString &value);
    int get(int at,NnString &value);
    int seekLineStartsWith( const NnString &key, NnString &line);
    int seekLineHas(        const NnString &key, NnString &line);
    void read( Reader &reader );
};

#endif
