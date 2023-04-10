# CeFlo interpreter

### ---------------------------------------------------------------------------------------------------------------

# Contact / help

> Developer's GitHub -> YxnnXriel  

or  

Contact me by email yannariel13@gmail.com for help, ask questions or contribute to the development of this small project.

# Examples


> les types de variables 
```c
    caractère -> char
    boolean -> bool
    integer -> int
    floatant -> float
    liste - > list
    type complexe -> struct

```


> variable declaration
```typescript

    var variable = 21;

```



> conditions

```js
    var nbr = 18

    if nbr < 18  {
        println("you are underage ");
    }
    else if nbr == 18 {
        println("you have just entered adulthood ");
    }
    else {
        println("you are of legal age");
    }
```


> loop
```js
    var mult = 2;
    var i = 0;
    
    while i <= 10:{
        println(mult," x ",i," = ",i*mult);
        i+=1;
    }

```

> the lists

```js

    var liste = [1,2,3,4,"Ceflo"];

    // [ 1 , 2 , 3 , 4 , "Ceflo" ]
    println(liste);

```

> functions and procedures

```go

    func SayHello():{
        println("Hello, world");
    }

    func GetNumber2():{
        return 1+1;
    }

    SayHello();

    println(GetNumber2());
    
```

> complex types (like struct in C)

```go

    struct Personne:{
        
        var Nom::str
        var Prenom::str
        
        func __init__(name,subname){
            Nom = name
            Prenom = subname
        }


        // This function should take nothing as parameters and should return nothing.
        func __str__(){
            return Nom + "  " + Prenom;
        }


        func sayHello(){
            println("Hello");
        }
    }


    var p1::Personne = new Personne("John","Doe");

    println(p1); // John  Doe

```



# Next release and other

    you can contact me if you want to help with this little interpreter. see contact / help section
