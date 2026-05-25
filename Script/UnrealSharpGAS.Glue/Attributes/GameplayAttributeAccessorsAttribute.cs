namespace UnrealSharp.UnrealSharpGAS;

[AttributeUsage(AttributeTargets.Property)]
public class GameplayAttributeAccessorsAttribute : Attribute
{
    public bool GetAttribute { get; }
    public bool Get { get; }
    public bool Set { get; }
    public bool Init { get; }

    public GameplayAttributeAccessorsAttribute(
        bool getAttribute = true,
        bool get = true,
        bool set = true,
        bool init = true)
    {
        GetAttribute = getAttribute;
        Get = get;
        Set = set;
        Init = init;
    }
}
