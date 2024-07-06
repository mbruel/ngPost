## NgPost C++/Qt Coding rules
<br/>

### I General class formatting

- Use CamelCase everywhere like C++ standard
- Classes always starts with a capital. Methods with a lower case
- No more `#define`for constants, use `constexpr` instead
- for prepocessing macro (`#ifdef`) define them in CMakeList (or .pro)
- use namespaces to classify the code and avoid too long class names
- use a **CONF** namepace for constants. they start with a **k** then a capital. use **constexpr** for fundamental types, **inline** for others. (cf [cpp-17-inline-variables](https://www.geeksforgeeks.org/cpp-17-inline-variables/)) <br/>Shared one should go in a specific header (conf.h or dbConf.h...) to avoid include a class when it is not used except for its constants
- for pointers, the * should touch the variable not its type. Why cause if you would decalare several pointers you'd do `int *p1 = &v1, *p2 = nullptr;` with or without initialisation
- always use **const*** when the pointer can't be changed. but if that's case prefer using a reference.
- `*const*` should be replaced by `const&` when it can
- use RAII ([Resource Acquisition Is Initialization](https://en.cppreference.com/w/cpp/language/raii)) as often as possible
- always use ++i or ++it rather than i++ or it++ except if good reason
- carefull when choosing a containor. Often prefer a Hash to a Map (except if ordering really  matters). only use Vectors if a max size is know to avoid realloc. so use the `reserve` method asap
- for Services or Tools use pure static class could inherit from

```
class PureStaticClass
{
    PureStaticClass() = delete;

    PureStaticClass(PureStaticClass const &)  = delete;
    PureStaticClass(PureStaticClass const &&) = delete;

    PureStaticClass &operator=(PureStaticClass const &)  = delete;
    PureStaticClass &operator=(PureStaticClass const &&) = delete;
};
```

- Singleton could derives from:  
the initialiazation could be done in main in the expected order. The reset method is for macro tests purposes. the instance could a reference or const& instead of a pointer.

```
template <typename T>
class Singleton
{
protected:
    static T *sInstance; //!< Unique instance

protected:
    // Constructor / Destructor are protected so we can inherit
    Singleton()          = default;
    virtual ~Singleton() = default;

    virtual void connectSignalSlots() { }

public:
    Singleton(Singleton const &)             = delete;
    Singleton(Singleton const &&)            = delete;
    Singleton &operator=(Singleton const &)  = delete;
    Singleton &operator=(Singleton const &&) = delete;

    static T const &instance() { return *sInstance; }

    //! should be called at the beginning of main.cpp
    static void createInstance()
    {
        if (!sInstance)
            sInstance = new T;
        sInstance->connectSignalSlots();
    }

    static bool isCreated() { return sInstance != nullptr; }

    static void reset()
    {
        if (sInstance)
        {
            delete sInstance;
            sInstance = nullptr;
        }
    }
};

template <typename T>
T *Singleton<T>::sInstance = nullptr;
```

<br/>

### II Header
#### II.1 includes
1. includes from standard C++
2. then Qt ones
3. all class forwardings for pointers (avoid unnecessary includes in header)
4. project headers dependancies

Separate those 4 includes blocs with an empty line => usage of clang-format allow to have them in alphabetical order
<br/>


#### II.2 **CONF** namespace for business constant variable
rather than static const
<br/>


#### II.3 class definition
1. Q_OBJECT or Q_GADJECT macro
2. friend classes then functions
3. **signals** : prefix: **sig** => we see first how the QObject can communicate with others. 
4. **enums** (either public or private) shared one that doesn't need the rest of the class should go in **CONF** namespace in a common header
5. **members**. Always private!. **starts with an underscore** and lower case (lazyness: avoiding a capital at the beginning so the dev don't have to use `Shift` <br/>
except static members start with a **s** than a capital <br />
If possible short doxygen comment on same line using `//!< short description`
6. members pointers that we don't own could have a **ptr** prefix or maybe better a **Ptr** suffix (lazyness for auto completion and no `Shift` usage)
7. **public methods** so with first Constructors followed by Destructor then the others. (as said before they start with a lower case) no prefix by lazyness (avoid `Shift`)
8. **public slots**: prefix: **on** then capital. explicit name.
9. protected and private slots.
10. protected and private methods. private methods that does specific implementation could start with an underscore.
<br/>

**static const class member** should either be **constexpr** or **`inline** with their definition in the header => we don't have to jump in the cpp to see the values.

Avoid pointers for heap members that we don't own and prefer reference.

Always use `const &` return when possible.  Same for methods attributes. prefer it to pointer (avoid unintialized pointer)

<br/>

### III CPP
1. includes : same rules than in the header except no more class forwarding! avoid unecessary includes
2. namepace if used. To avoid `void Namespace::MyClass::method(){`
3. start with Constructors then Destructor
4. methods following header order

#### III.1 Constructors
- members have one line each (if many) with the coma at the begining. why: in case some attributes are preprocessor conditionned (`#ifdef ... #endif`)
- obviously in the right construction order
- pointers always initialized to nullptr if not from a constructor argument
- avoid initializing members that use default constructor
- fundamental types always initialized with their default value either 0 or something else
- don't rely in Qt parenting except for GUI objects
- RAII as said in introduction as often as possible



#### III.2 methods

- never declare variable before being used (not necessary)
- return case (or throw) at soon as possible to avoid **if** indentation or imbricated **if**<br/>
better for readability and execution time<br/>
```
if (!QFile(path).exist())  
{
    log sth ?
    return; // or throw
}
Now nominal case now`
...
```

- short methods, should fill on a screen => cut properly using private sub methods (that could start with an underscore)
 

- for loops: use c++11 for loop range using `auto const&` as often as possible. Otherwise, avoid recalculating stop condition at each run: 
```
for (auto it = map.cbegin(), itEnd = map.cend(); it != itEnd; ++it)
or
for (int i = 0, end = vector.size() ; i != end; ++i)
```





