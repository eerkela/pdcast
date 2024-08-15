module;
#include "__init__.h"
export module bertrand.python;


export using ::PyObject;


export namespace py {
    using py::Interpreter;
    using py::Arg;
    using py::arg;
    using py::Handle;
    using py::WeakRef;
    using py::Capsule;
    using py::Buffer;
    using py::MemoryView;
    using py::Object;
    using py::Function;
    using py::Type;
    using py::Super;
    using py::Code;
    using py::Frame;
    using py::Module;
    using py::NoneType;
    using py::NotImplementedType;
    using py::EllipsisType;
    using py::Bool;
    using py::Int;
    using py::Float;
    using py::Complex;
    using py::Str;
    using py::Bytes;
    using py::ByteArray;
    using py::Timezone;
    using py::Date;
    using py::Time;
    using py::Datetime;
    using py::Timedelta;
    using py::Slice;
    using py::Range;
    using py::List;
    using py::Tuple;
    using py::Set;
    using py::FrozenSet;
    using py::Dict;
    using py::KeyView;
    using py::ValueView;
    using py::ItemView;
    using py::MappingProxy;

    using py::Exception;
    using py::ArithmeticError;
    using py::FloatingPointError;
    using py::OverflowError;
    using py::ZeroDivisionError;
    using py::AssertionError;
    using py::AttributeError;
    using py::BufferError;
    using py::EOFError;
    using py::ImportError;
    using py::ModuleNotFoundError;
    using py::LookupError;
    using py::IndexError;
    using py::KeyError;
    using py::MemoryError;
    using py::NameError;
    using py::UnboundLocalError;
    using py::OSError;
    using py::BlockingIOError;
    using py::ChildProcessError;
    using py::ConnectionError;
    using py::BrokenPipeError;
    using py::ConnectionAbortedError;
    using py::ConnectionRefusedError;
    using py::ConnectionResetError;
    using py::FileExistsError;
    using py::FileNotFoundError;
    using py::InterruptedError;
    using py::IsADirectoryError;
    using py::NotADirectoryError;
    using py::PermissionError;
    using py::ProcessLookupError;
    using py::TimeoutError;
    using py::ReferenceError;
    using py::RuntimeError;
    using py::NotImplementedError;
    using py::RecursionError;
    using py::StopAsyncIteration;
    using py::StopIteration;
    using py::SyntaxError;
    using py::IndentationError;
    using py::TabError;
    using py::SystemError;
    using py::TypeError;
    using py::CastError;
    using py::ReferenceCastError;
    using py::ValueError;
    using py::UnicodeError;
    using py::UnicodeDecodeError;
    using py::UnicodeEncodeError;
    using py::UnicodeTranslateError;

    using py::Disable;
    using py::Returns;
    using py::__as_object__;
    using py::__isinstance__;
    using py::__issubclass__;
    using py::__init__;
    using py::__explicit_init__;
    using py::__cast__;
    using py::__explicit_cast__;
    using py::__call__;
    using py::__getattr__;
    using py::__setattr__;
    using py::__delattr__;
    using py::__getitem__;
    using py::__setitem__;
    using py::__delitem__;
    using py::__len__;
    using py::__iter__;
    using py::__reversed__;
    using py::__contains__;
    using py::__hash__;
    using py::__abs__;
    using py::__invert__;
    using py::__pos__;
    using py::__neg__;
    using py::__increment__;
    using py::__decrement__;
    using py::__lt__;
    using py::__le__;
    using py::__eq__;
    using py::__ne__;
    using py::__ge__;
    using py::__gt__;
    using py::__add__;
    using py::__iadd__;
    using py::__sub__;
    using py::__isub__;
    using py::__mul__;
    using py::__imul__;
    using py::__truediv__;
    using py::__itruediv__;
    using py::__floordiv__;
    using py::__ifloordiv__;
    using py::__mod__;
    using py::__imod__;
    using py::__pow__;
    using py::__ipow__;
    using py::__lshift__;
    using py::__ilshift__;
    using py::__rshift__;
    using py::__irshift__;
    using py::__and__;
    using py::__iand__;
    using py::__or__;
    using py::__ior__;
    using py::__xor__;
    using py::__ixor__;

    using py::reinterpret_borrow;
    using py::reinterpret_steal;
    using py::as_object;
    using py::isinstance;
    using py::issubclass;
    using py::hasattr;
    using py::getattr;
    using py::setattr;
    using py::delattr;
    using py::print;
    using py::repr;
    using py::hash;
    using py::len;
    using py::size;
    using py::iter;
    using py::begin;
    using py::cbegin;
    using py::end;
    using py::cend;
    using py::reversed;
    using py::rbegin;
    using py::crbegin;
    using py::rend;
    using py::crend;
    using py::abs;
    using py::pow;
    using py::Round;
    using py::div;
    using py::mod;
    using py::divmod;
    using py::round;
    using py::assert_;
    using py::visit;
    using py::transform;
    using py::callable;
    using py::all;
    using py::any;
    using py::enumerate;
    using py::filter;
    using py::map;
    using py::max;
    using py::min;
    using py::next;
    using py::sum;
    using py::zip;
    using py::builtins;
    using py::globals;
    using py::locals;
    using py::aiter;
    using py::anext;
    using py::ascii;
    using py::bin;
    using py::chr;
    using py::dir;
    using py::eval;
    using py::exec;
    using py::hex;
    using py::id;
    using py::oct;
    using py::ord;
    using py::vars;

    using py::operator~;
    using py::operator<;
    using py::operator<=;
    using py::operator==;
    using py::operator!=;
    using py::operator>=;
    using py::operator>;
    using py::operator+;
    using py::operator-;
    using py::operator++;
    using py::operator--;
    using py::operator+=;
    using py::operator-=;
    using py::operator*;
    using py::operator*=;
    using py::operator/;
    using py::operator/=;
    using py::operator%;
    using py::operator%=;
    using py::operator<<;
    using py::operator<<=;
    using py::operator>>;
    using py::operator>>=;
    using py::operator&;
    using py::operator&=;
    using py::operator|;
    using py::operator|=;
    using py::operator^;
    using py::operator^=;

    using py::True;
    using py::False;
    using py::None;
    using py::NotImplemented;
    using py::Ellipsis;
}

export namespace std {
    using std::hash;
    using std::equal_to;
}


// extern "C" PyObject* PyInit_python() {
//     return Module<"bertrand.python">::__python__::__export__();
// }
