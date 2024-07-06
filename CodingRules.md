## NgPost C++/Qt Coding rules

### I General class formatting
- Use CamelCase everywhere like C++ standard
- Classes always starts with a capital. Methods with a lower case
- No more `#define`for constants, use `constexpr` instead
- for prepocessing macro (`#ifdef`) define them in CMakeList (or .pro)
- use namespaces to classify the code and avoid too long class names
- use a **CONF** namepace for constants. they start with a **k** then a capital. use **constexpr** for fundamental types, **inline** for others. (cf [cpp-17-inline-variables](https://www.geeksforgeeks.org/cpp-17-inline-variables/)) <br/>Shared one should go in a specific header (conf.h or dbConf.h...) to avoid include a class when it is not used except for its constants
<br/>

#### I.1 Header
##### I.1.1 includes
1. includes from standard C++
2. then Qt ones
3. all class forwardings for pointers (avoid unnecessary includes in header)
4. project headers dependancies

Separate those 4 includes blocs with an empty line => usage of clang-format allow to have them in alphabetical order
<br/>


##### I.1.2 **CONF** namespace for business constant variable
rather than static const
<br/>


##### I.1.3 class definition
1. Q_OBJECT or Q_GADJECT macro
2. friend classes then functions
3. **signals** : prefix: **sig** => we see first how the QObject can communicate with others. 
4. **members**. Always private!. **starts with an underscore** and lower case (lazyness: avoiding a capital at the beginning so the dev don't have to use `Shift` <br/>
except static members start with a **s** than a capital <br />
If possible short doxygen comment on same line using `//!< short description`
5. **public methods** so with first Constructors followed by Destructor then the others. (as said before they start with a lower case) no prefix by lazyness (avoid `Shift`)
6. **public slots**: prefix: **on** then capital. explicit name.
7. protected and private slots.
8. protected and private methods. private methods that does specific implementation could start with an underscore.


** static const class member** should either be **constexpr** or **inline** with their definition in the header => we don't have to jump in the cpp to see the values.

Avoid pointers for heap members that we don't own and prefer reference.

Always use `const &` return when possible.  Same for methods attributes. prefer it to pointer (avoid unintialized pointer)






