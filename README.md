# UnrealSharpGAS

Work in progress `GameplayAbilitySystem` bindings for C# using UnrealSharp.

Documentation will come soon.

## Setup

1. Clone this repo into `YourProject/Plugins/UnrealSharpGAS`
2. Compile the C++ solution in your IDE
3. In your C# game code project (e.g. `ManagedGame.csproj`), import the shared props:

```xml
<Import Project="..\..\Plugins\UnrealSharpGAS\UnrealSharpGAS.Shared.props" />
```

## FCSGameplayAttributeData

`[GameplayAttributeAccessors]` is a Roslyn source generator that automatically creates strongly-typed accessors for each `FCSGameplayAttributeData` property, eliminating boilerplate.

## Per-property flags

Decorate individual properties to control which methods are generated:

| Parameter    | Default | Generated method                                 |
|--------------|---------|--------------------------------------------------|
| getAttribute | `true`  | `static FGameplayAttribute Get{X}Attribute()`    |
| get          | `true`  | `float Get{X}()`                                 |
| set          | `true`  | `void Set{X}(float NewValue)`                    |
| init         | `true`  | `void Init{X}(float NewValue)`                   |

```csharp
[UClass]
public partial class UMyAttributeSet : UCSAttributeSet
{
    // Only GetHealthAttribute() + InitHealth()
    [UProperty]
    [GameplayAttributeAccessors(true, false, false, true)]
    public partial FCSGameplayAttributeData Health { get; set; }

    // Get + Set only (no static attribute lookup, no init)
    [UProperty] 
    [GameplayAttributeAccessors(false, true, true, false)]
    public partial FCSGameplayAttributeData Stamina { get; set; }

    // All four accessors
    [UProperty, GameplayAttributeAccessors]
    public partial FCSGameplayAttributeData Mana { get; set; }
}
```
