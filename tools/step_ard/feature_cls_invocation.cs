// Argument normalization only. Calls the checked public CLS MethodInfo unchanged.
using System;
using System.Reflection;
using System.Management.Automation;

public static class FeatureClsInvocation
{
    public static object Call(MethodInfo method, object[] arguments)
    {
        if (method == null || !method.IsPublic || !method.IsStatic)
            throw new ArgumentException("Expected a public static method");
        ParameterInfo[] parameters = method.GetParameters();
        if (arguments == null) arguments = new object[0];
        if (parameters.Length != arguments.Length)
            throw new ArgumentException("Argument count mismatch: " + method.Name);
        object[] native = new object[arguments.Length];
        for (int i = 0; i < native.Length; i++)
        {
            object value = arguments[i];
            for (int depth = 0; value is PSObject && depth < 16; depth++)
                value = ((PSObject)value).BaseObject;
            if (value == null || value.GetType() != parameters[i].ParameterType)
                throw new ArgumentException("Argument " + i + " type mismatch: " + method.Name);
            native[i] = value;
        }
        return method.Invoke(null, native);
    }

    public static bool CheckArguments(string text, DayOfWeek choice, int number)
    {
        return text == "CLS argument check" && choice == DayOfWeek.Friday && number == -1;
    }

    public static bool SelfTest()
    {
        MethodInfo method = typeof(FeatureClsInvocation).GetMethod("CheckArguments");
        object[] wrapped = { new PSObject("CLS argument check"),
                             new PSObject(DayOfWeek.Friday), new PSObject(-1) };
        if (!(bool)Call(method, wrapped)) return false;
        try { Call(method, new object[] { "CLS argument check", 5, -1 }); }
        catch (ArgumentException) { return true; }
        return false;
    }
}
