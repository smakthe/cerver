# Cerver

Cerver is a C-based backend HTTP server for scaffolding CRUD (Create, Read, Update, Delete) resources. It provides a streamlined way to generate model, controller, and route files for new resources, organized in a clean directory structure, and includes a basic multi-threaded HTTP server.

## Features

- **Guided Resource Scaffolding:**  
  Generate C source files for models, controllers, and routes based on user input with helpful prompts.
- **Organized Directory Structure:**  
  All files for a resource are created in a dedicated directory.
- **Basic HTTP Server:**  
  Multi-threaded server that listens on port 3000 and handles HTTP requests.
- **In-Memory Database (B+ Tree):**  
  Simple B+ tree implementation for primary key indexing.
- **ORM Layer:**  
  Basic object-relational mapping for defining models and associations.

## Project Structure

```
cerver/
│
├── main.c                        # Entry point, handles scaffolding and server startup
├── controllers/
│   ├── scaffold_controller.c     # Controller code generator and controller logic
│   └── scaffold_controller.h
├── models/
│   ├── scaffold_model.c          # Model code generator
│   └── scaffold_model.h
├── routes/
│   ├── scaffold_routes.c         # Route code generator
│   └── scaffold_routes.h
├── server/
│   ├── http_server.c             # HTTP server implementation
│   └── http_server.h
├── database/
│   ├── application/
│   │   ├── orm.c                 # ORM logic
│   │   └── orm.h
│   ├── logical/
│   │   ├── database.c            # Database logic
│   │   └── database.h
│   └── physical/
│       ├── b_plus_tree.c         # B+ tree implementation
│       └── b_plus_tree.h
└── scaffolded_resources/         # Generated resources are stored here
    └── {resource_name}/          # Each resource has its own directory
        ├── {resource_name}.c              # Resource model
        ├── {resource_name}_controller.c   # Resource controller
        ├── {resource_name}_routes.c       # Resource routes
        └── {resource_name}.dat            # Resource database file
```

## Getting Started

### Prerequisites

- GCC (or any C compiler)
- pthreads (usually included on Unix-like systems)

### Build

Build the project using:

```sh
make
```

### Run

Start the application:

```sh
./cerver
```

## Resource Scaffolding

When you run the application, you will be guided through the scaffolding process:

1. **Enter the resource name** (e.g., `Book`, `Customer`, `Product`):
   - Resource names can be entered in any case (uppercase, lowercase, mixed)
   - All generated files will use lowercase in their filenames for consistency

2. **Enter the resource attributes** in the format `name:type,another_name:type,...`
   
   Supported attribute types:
   - `int` - Integer
   - `float` - Floating point number
   - `double` - Double precision floating point
   - `char` - Single character
   - `string` - Character string (implemented as char array)
   - `bool` - Boolean value

### Example

```
Enter the resource name (e.g., 'Book'): Book
Enter the resource attributes in the format (name:type,another_name:type,...): title:string,price:float,pages:int,author:string

Creating resource 'Book' with 4 attributes...

Resource 'Book' has been successfully created!
Files created in: /Users/somak/cerver/scaffolded_resources/book
  - /Users/somak/cerver/scaffolded_resources/book/book.c
  - /Users/somak/cerver/scaffolded_resources/book/book_controller.c
  - /Users/somak/cerver/scaffolded_resources/book/book_routes.c

API endpoints available:
  GET    /book      - List all books
  GET    /book/:id  - Get a specific book by ID
  POST   /book      - Create a new book
  PATCH  /book/:id  - Update a book
  DELETE /book/:id  - Delete a book
```

### Generated Files

All files for a resource are created in `/Users/somak/cerver/scaffolded_resources/{resource_name}/`:

1. **Model File** (`{resource_name}.c`):
   - Contains the struct definition based on your attributes
   - CRUD function stubs for your resource

2. **Controller File** (`{resource_name}_controller.c`):
   - Contains controller functions for handling resource operations
   - Includes index, view, create, update, and destroy operations

3. **Routes File** (`{resource_name}_routes.c`):
   - Contains HTTP route handlers for your resource
   - Maps HTTP methods to corresponding controller functions

4. **Database File** (`{resource_name}.dat`):
   - Binary file for storing resource data
   - Used by the B+ tree indexing system

### Accessing the Server

After scaffolding, the HTTP server starts on port 3000. You can test it with curl:

```sh
curl http://localhost:3000/
```

## API Endpoints

For each scaffolded resource (e.g., "book"), the following RESTful endpoints are available:

- **List All** (GET `/book`)
- **View One** (GET `/book/:id`)
- **Create** (POST `/book`)
- **Update** (PATCH `/book/:id`)
- **Delete** (DELETE `/book/:id`)

## Extending the Project

1. **Implement CRUD Logic:**
   - The scaffolded files provide the structure - add your business logic to each controller function

2. **Enhance the HTTP Server:**
   - Modify `server/http_server.c` to properly route requests to your controllers

3. **Improve Data Validation:**
   - Add input validation for request parameters

4. **Add Error Handling:**
   - Implement more robust error handling throughout the application

5. **Extend Database Features:**
   - Add more sophisticated querying capabilities to the ORM layer