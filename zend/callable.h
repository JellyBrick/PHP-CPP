/**
 *  Callable.h
 *
 *  Object represents a callable function or method that is defined with the CPP
 *  API.
 *
 *  @author Emiel Bruijntjes <emiel.bruijntjes@copernica.com>
 *  @copyright 2013 Copernica BV
 */

/**
 *  Dependencies
 */
#include <cstring>

/**
 *  Set up namespace
 */
namespace Php {

/**
 *  Class definition
 */
class Callable
{
public:
    /**
     *  Constructor
     *
     *  @param  callback    The callback to invoke
     *  @param  name        Function or method name
     *  @param  arguments   Information about the arguments
     */
    Callable(ZendCallback callback, const char *name, const Arguments &arguments = {}) :
        _callback(callback),
        _name(name),
        _argc(arguments.size()),
        _argv(new zend_internal_arg_info[_argc + 2])
    {
        // the first record is initialized with information about the function,
        // so we skip that here
        int i=1;

        // loop through the arguments
        for (auto &argument : arguments)
        {
            // increment counter with number of required parameters
            if (argument.required()) _required++;

            // fill the arg info
            fill(&_argv[i++], argument);
        }

        // initialize the extra argument
        _argv[i].type = ZEND_TYPE_ENCODE_CLASS(nullptr, 0);
        _argv[i].name       = nullptr;
    }

    /**
     *  Constructor
     *
     *  @param  name        Function or method name
     *  @param  arguments   Information about the arguments
     */
    Callable(const char *name, const Arguments &arguments = {}) :
        Callable(nullptr, name, arguments) {}

    /**
     *  Copy constructor
     *  @param  that
     */
    Callable(const Callable &that) :
        _name(that._name),
        _return(that._return),
        _required(that._required),
        _argc(that._argc),
        _argv(nullptr) {}
        // @todo: we have no arguments after copy? is this correct?
        // we do have the argument count though...

    /**
     *  Move constructor
     *  @param  that
     */
    Callable(Callable &&that) :
        _name(std::move(that._name)),
        _return(that._return),
        _required(that._required),
        _argc(that._argc),
        _argv(std::move(that._argv)) {}

    /**
     *  Destructor
     */
    virtual ~Callable() = default;

    /**
     *  Method that gets called every time the function is executed
     *  @param  params      The parameters that were passed
     *  @return Variable    Return value
     */
    virtual Value invoke(Parameters &params) = 0;

    /**
     *  Fill a function entry
     *  @param  entry       Entry to be filled
     *  @param  ns          Active namespace
     *  @param  classname   Optional class name
     *  @param  flags       Access flags
     */
    void initialize(zend_function_entry *entry, const char *classname = nullptr, int flags = 0) const;

    /**
     *  Fill function info
     *  @param  info        Info object to be filled
     *  @param  ns          Active namespace
     *  @param  classname   Optional class name
     */
    void initialize(zend_internal_function_info *info, const char *classname = nullptr) const;


protected:

    /**
     *  The callback to invoke
     *  @var    ZendCallback
     */
    ZendCallback _callback;

    /**
     *  Name of the function
     *  @var    std::string
     */
    std::string _name;

    /**
     *  Suggestion for the return type
     *  @var    Type
     */
    Type _return = Type::Undefined;

    /**
     *  Required number of arguments
     *  @var    uint32_t
     */
    uint32_t _required = 0;

    /**
     *  Total number of arguments
     *  @var    integer
     */
    int _argc = 0;

    /**
     *  The arguments
     *  @var std::unique_ptr<zend_internal_arg_info[]>
     */
    std::unique_ptr<zend_internal_arg_info[]> _argv;

    /**
     *  Private helper method to fill an argument object
     *  @param  info        object from the zend engine
     *  @param  arg         original object
     */
    void fill(zend_internal_arg_info *info, const Argument &arg) const
    {
        // fill members
        info->name = arg.name();

        // are we filling an object
        if (arg.type() == Type::Object) info->type = ZEND_TYPE_ENCODE_CLASS(arg.classname(), arg.allowNull());
        else info->type = ZEND_TYPE_ENCODE_CLASS(nullptr, arg.allowNull());

        // set the correct type-hint
        switch (arg.type())
        {
            case Type::Undefined:   info->type = ZEND_TYPE_ENCODE(IS_UNDEF, arg.allowNull());     break;  // undefined means we'll accept any type
            case Type::Null:        info->type = ZEND_TYPE_ENCODE(IS_UNDEF, arg.allowNull());     break;  // this is likely an error, what good would accepting NULL be? accept anything
            case Type::False:       info->type = ZEND_TYPE_ENCODE(_IS_BOOL, arg.allowNull());     break;  // accept true as well ;)
            case Type::True:        info->type = ZEND_TYPE_ENCODE(_IS_BOOL, arg.allowNull());     break;  // accept false as well
            case Type::Bool:        info->type = ZEND_TYPE_ENCODE(_IS_BOOL, arg.allowNull());     break;  // any bool will do, true, false, the options are limitless
            case Type::Numeric:     info->type = ZEND_TYPE_ENCODE(IS_LONG, arg.allowNull());      break;  // accept integers here
            case Type::Float:       info->type = ZEND_TYPE_ENCODE(IS_DOUBLE, arg.allowNull());    break;  // floating-point values welcome too
            case Type::String:      info->type = ZEND_TYPE_ENCODE(IS_STRING, arg.allowNull());    break;  // accept strings, should auto-cast objects with __toString as well
            case Type::Array:       info->type = ZEND_TYPE_ENCODE(IS_ARRAY, arg.allowNull());     break;  // array of anything (individual members cannot be restricted)
            case Type::Object:      info->type = ZEND_TYPE_ENCODE(IS_OBJECT, arg.allowNull());    break;  // must be an object of the given classname
            case Type::Callable:    info->type = ZEND_TYPE_ENCODE(IS_CALLABLE, arg.allowNull());  break;  // anything that can be invoked
            default:                info->type = ZEND_TYPE_ENCODE(IS_UNDEF, arg.allowNull());     break;  // if not specified we allow anything

        }

        // from PHP 5.6 and onwards, an is_variadic property can be set, this
        // specifies whether this argument is the first argument that specifies
        // the type for a variable length list of arguments. For now we only
        // support methods and functions with a fixed number of arguments.
        info->is_variadic = false;

        // this parameter is a regular type
        info->type = ZEND_TYPE_ENCODE(0, arg.allowNull());
        info->pass_by_reference = arg.byReference();
    }

    /**
     *  Function that is called by the Zend engine every time that a function gets called
     *  @param  ht
     *  @param  return_value
     *  @param  return_value_ptr
     *  @param  this_ptr
     *  @param  return_value_used
     *  @return integer
     */
    static void invoke(INTERNAL_FUNCTION_PARAMETERS);

};

/**
 *  End of namespace
 */
}

