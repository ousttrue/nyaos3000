#ifndef HISTORY_H
#define HISTORY_H

class Reader;

class History : public NnVector {
public:
    History &operator << ( const NnString & );
    NnString *operator[](int at);
    NnString *top(){
	return this->size() >= 1 ? (NnString*)NnVector::top() : NULL ; 
    }
    void drop(){ delete (NnString*)this->pop(); }
    void pack();
    int set(int at,NnString &value);
    int get(int at,NnString &value);
    int seekLineStartsWith( const NnString &key, NnString &line);
    int seekLineHas(        const NnString &key, NnString &line);
    void read( Reader &reader );
};

#endif
