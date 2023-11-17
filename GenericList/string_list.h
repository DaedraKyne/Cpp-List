#pragma once

class List_String {
    //note: elements are value copies of original objects (copy-by-value, not copy-by-reference)
public:
    List_String();

    friend void swap(List_String& first, List_String& second);

    //Rule of 3
    ~List_String(); //Destructor
    List_String(const List_String& other); //Copy Constructor
    List_String& operator=(const List_String& other); //Copy assignement

    //Rule of 5
    //R-value references (&&) explained: http://thbecker.net/articles/rvalue_references/section_01.html 
    //basic explanation: if a function argument is &&, whatever it references will stop existing at the end of the function
    List_String(List_String&& other);
    List_String& operator=(List_String&& other);




    int Capacity() const;
    bool Capacity(int new_capacity);
    int Count() const;

    std::string ToString() const;

    bool Add(const std::string& new_val);
    bool Contains(const std::string& val) const;
    bool RemoveAt(int index);

    int IndexOf(const std::string& val) const;
    bool Remove(const std::string& val);

    const std::string& operator[](int index) const { return Get(index); }
    std::string& operator[](int index) { return Get(index); }
    const std::string& Get(int index) const;
    std::string& Get(int index);


    //can iterate through list without using iterators if internal data is contiguous
    std::string* begin() { return data; }
    const std::string* begin() const { return data; } //non-copy version for read-only
    const std::string* cbegin() const { return data; } //same as above, allows for direct call to cbegin for user who knows they want constant iterators
    
    std::string* end() { return data + count; }
    const std::string* end() const { return data + count; }
    const std::string* cend() const { return data + count; }



private:
    std::string* data;
    int capacity;
    int count;

    static std::string* CreateDeepCopy(std::string* data, size_t data_size, size_t copy_size);


};

void Main_Test_List_String();