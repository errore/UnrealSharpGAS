namespace UnrealSharp.UnrealSharpGAS;

[AttributeUsage(AttributeTargets.Method)]
public sealed class AbilityTaskAttribute(string category = "Ability|Tasks") : Attribute
{
	public string Category => category;
}
