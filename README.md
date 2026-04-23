# Cerver

Cerver is a C-based backend HTTP server for scaffolding full CRUD (Create, Read, Update, Delete) resources. It provides a streamlined way to generate model, controller, and route files for new resources, organized in a clean directory structure, and includes a multi-threaded HTTP server backed by a file-persistent B+ tree database.

## Features

- **Guided Resource Scaffolding:**
  Interactively generate C source files for models, controllers, and routes based on user input.
- **Type-Safe Code Generation:**
  User-friendly types (`string`, `text`, `boolean`, `date`, `float`, `int`, etc.) are mapped to correct C declarations automatically.
- **Organized Directory Structure:**
  All files for a resource are created in a dedicated directory under `scaffolded_resources/`.
- **Multi-Threaded HTTP Server:**
  Listens on port 3000. Each incoming connection is handled in its own `pthread` thread.
- **File-Persistent Database (B+ Tree):**
  Each resource has its own `.dat` file. A B+ tree index provides O(log n) primary key lookups. Data survives process restarts.
- **ORM Layer:**
  Object-relational mapping for defining model schemas, creating instances, and performing CRUD operations against the physical storage layer.
- **RESTful Routing:**
  Five HTTP methods supported out of the box per resource: `GET` (list/view), `POST` (create), `PATCH` (partial update), `PUT` (full replace), `DELETE`.

## Project Structure

```
cerver/
│
├── main.c                                # Entry point: scaffolding prompt and server startup
├── controllers/
│   ├── scaffold_controller.c             # Runtime CRUD logic + controller code generator
│   └── scaffold_controller.h
├── models/
│   ├── scaffold_model.c                  # Model code generator
│   ├── scaffold_model.h
│   └── model_setup.c / model_setup.h     # Central model registry (register_model, find_model_by_name)
├── routes/
│   ├── scaffold_routes.c                 # Route code generator + wildcard HTTP dispatcher
│   └── scaffold_routes.h
├── utils/
│   ├── path_utils.c / path_utils.h       # Portable path construction utilities
│   └── type_map.c / type_map.h           # Scaffold-type → C-type mapping
├── server/
│   ├── http_server.c                     # HTTP server, request parser, response sender, router
│   └── http_server.h
├── database/
│   ├── rdbms.c / rdbms.h                 # High-level database API (db_system_init, db_save, etc.)
│   ├── application/
│   │   ├── orm.c / orm.h                 # ORM: Model schema, ModelInstance, CRUD operations
│   ├── logical/
│   │   ├── database.c / database.h       # Table management, row insert/read/update/delete, compaction
│   └── physical/
│       ├── b_plus_tree.c                 # B+ tree: insert, search, delete, leaf scan
│       └── b_plus_tree.h
└── scaffolded_resources/                 # Generated resources live here (git-ignored in production)
    └── {resource_name}/
        ├── {resource_name}.c             # Model: struct definition + CRUD functions
        ├── {resource_name}.h             # Model header: struct + function prototypes
        ├── {resource_name}_controller.c  # Controller: thin wrappers delegating to runtime
        ├── {resource_name}_routes.c      # Routes: per-resource HTTP handlers + register function
        └── {resource_name}.dat           # Binary data file used by the B+ tree index
```

## Getting Started

### Prerequisites

- GCC (or any C99-compatible compiler)
- pthreads (included on all Unix-like systems)

### Build

```sh
make
```

### Run

```sh
./cerver
```

## Resource Scaffolding

When you run the application, you are guided through the scaffolding process:

1. **Enter the resource name** (e.g., `book`, `user`, `product`):
   - Names can be entered in any case; all generated filenames use lowercase.

2. **Enter resource attributes** in the format `name:type,another_name:type,...`

   Supported attribute types:

   | Type      | C declaration         | Notes                        |
   |-----------|-----------------------|------------------------------|
   | `int`     | `int`                 |                              |
   | `float`   | `float`               |                              |
   | `double`  | `double`              |                              |
   | `char`    | `char`                | Single character             |
   | `string`  | `char field[256]`     | Short text                   |
   | `text`    | `char field[1024]`    | Long text                    |
   | `boolean` | `int`                 | 0 = false, 1 = true          |
   | `date`    | `char field[11]`      | Format: `YYYY-MM-DD`         |

   > **Important:** The first attribute is treated as the primary key and must be of type `int`.

### Example Session

```
Enter the resource name (singular form, e.g., 'book', 'user', 'product'): book
Enter the resource attributes: id:int,title:string,price:float,pages:int,author:string

Creating resource 'book' with 5 attributes...

Resource 'book' has been successfully created!
Files created in: cerver/scaffolded_resources/book
  - cerver/scaffolded_resources/book/book.c
  - cerver/scaffolded_resources/book/book.h
  - cerver/scaffolded_resources/book/book_controller.c
  - cerver/scaffolded_resources/book/book_routes.c

API endpoints available:
  GET    /books      - List all books
  GET    /book/:id   - Get a specific book by ID
  POST   /book      - Create a new book
  PATCH  /book/:id   - Partially update a book
  PUT    /book/:id   - Fully replace a book
  DELETE /book/:id   - Delete a book
```

### Generated Files

All files for a resource are created in `cerver/scaffolded_resources/{resource_name}/`:

1. **Model File** (`{resource_name}.c` + `{resource_name}.h`):
   - Struct definition with correct C types derived from your attribute declarations.
   - `create_`, `view_`, `update_`, and `destroy_` functions wired to the ORM.
   - `get_model_schema()` looks up the central model registry — no duplicate table registration.

2. **Controller File** (`{resource_name}_controller.c`):
   - Thin delegation wrappers (`indx_`, `view_ctrl_`, `create_ctrl_`, `update_ctrl_`, `replace_ctrl_`, `destroy_ctrl_`) that forward to the runtime CRUD functions in `scaffold_controller.c`.

3. **Routes File** (`{resource_name}_routes.c`):
   - Five static handler functions for `GET` (list), `GET` (view), `POST`, `PATCH`, `PUT`, `DELETE`.
   - A `register_{resource}_routes()` function that calls `register_route()` for each endpoint.

4. **Database File** (`{resource_name}.dat`):
   - File-backed row storage used by the B+ tree index.
   - Rows are soft-deleted (marked with `#`) and physically removed only during compaction.

## API Endpoints

For each scaffolded resource (e.g., `book`), the following RESTful endpoints are available:

| Method   | Path         | Action         | Description                                                  |
|----------|--------------|----------------|--------------------------------------------------------------|
| `GET`    | `/books`     | index          | Return a JSON array of all records                           |
| `GET`    | `/book/:id`  | view           | Return a single record by primary key                        |
| `POST`   | `/book`      | create         | Create a new record from a JSON body                         |
| `PATCH`  | `/book/:id`  | update         | Partially update a record — only supplied fields are changed |
| `PUT`    | `/book/:id`  | replace        | Fully replace a record — missing fields are cleared          |
| `DELETE` | `/book/:id`  | destroy        | Delete a record by primary key                               |

### Example `curl` Requests

```sh
# Create
curl -X POST http://localhost:3000/book \
  -H "Content-Type: application/json" \
  -d '{"id": 1, "title": "The C Programming Language", "price": 39.99, "pages": 272, "author": "Kernighan"}'

# List all
curl http://localhost:3000/books

# View one
curl http://localhost:3000/book/1

# Partial update
curl -X PATCH http://localhost:3000/book/1 \
  -H "Content-Type: application/json" \
  -d '{"price": 29.99}'

# Full replace
curl -X PUT http://localhost:3000/book/1 \
  -H "Content-Type: application/json" \
  -d '{"id": 1, "title": "The C Programming Language", "price": 19.99, "pages": 272, "author": "Ritchie"}'

# Delete
curl -X DELETE http://localhost:3000/book/1
```

## Architecture Overview

```
HTTP Request
     │
     ▼
http_server.c  (parse → match_pattern → route_request)
     │
     ▼
scaffold_routes.c  (wildcard dispatcher → handle_*_route)
     │
     ▼
scaffold_controller.c  (indx / view / create / update / replace / destroy)
     │
     ▼
model_setup.c  (find_model_by_name → Model schema)
     │
     ▼
orm.c  (ModelInstance lifecycle, save / find / delete)
     │
     ▼
database.c  (insert_row / read_row / update_row / delete_row)
     │
     ▼
b_plus_tree.c  (insert_key / search_key / delete_key / collect_all_keys)
     │
     ▼
{resource}.dat  (file-backed row storage)
```

## Extending the Project

1. **Add business logic:** Edit the generated controller file to add validation, computed fields, or side effects before delegating to the runtime functions.
2. **Add relationships:** Use `add_foreign_key()` in the ORM layer to declare foreign key metadata between models.
3. **Trigger compaction:** Call `db_compact_table("resource_name")` via the RDBMS API to reclaim disk space from soft-deleted rows.
4. **Add query support:** Extend `collect_all_keys` + the controller `indx` function with filtering predicates over the scanned rows.
5. **Change the port:** Edit `#define PORT 3000` in `server/http_server.c`.