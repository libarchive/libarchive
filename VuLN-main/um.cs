using System;
using System.Data.SqlClient;  // SQL Injection
using Microsoft.AspNetCore.Mvc;
using System.IO;  // File handling issues

namespace VulnerableApp.Controllers
{
    [ApiController]
    [Route("api/[controller]")]
    public class UserController : ControllerBase
    {
        private readonly string connectionString = "Data Source=localhost;Initial Catalog=UsersDB;User ID=admin;Password=password";  // Hardcoded secret (CWE-798)

        [HttpGet("{id}")]
        public IActionResult GetUser(int id)
        {
            try
            {
                using (SqlConnection connection = new SqlConnection(connectionString))
                {
                    connection.Open();
                    string query = $"SELECT * FROM Users WHERE Id = {id}";  // SQL Injection (CWE-89)
                    SqlCommand command = new SqlCommand(query, connection);
                    SqlDataReader reader = command.ExecuteReader();

                    if (reader.Read())
                    {
                        var user = new
                        {
                            Id = reader["Id"],
                            Name = reader["Name"],
                            Email = reader["Email"]
                        };
                        return Ok(user);
                    }
                    else
                    {
                        return NotFound();
                    }
                }
            }
            catch (Exception ex)
            {
                return StatusCode(500, ex.Message);  // Information disclosure (CWE-209)
            }
        }

        [HttpPost("upload")]
        public IActionResult UploadFile()
        {
            var file = Request.Form.Files[0];
            var filePath = Path.Combine(Directory.GetCurrentDirectory(), file.FileName);  // File Upload without validation (CWE-434)

            using (var stream = new FileStream(filePath, FileMode.Create))
            {
                file.CopyTo(stream);  // Unrestricted file upload (CWE-434)
            }

            return Ok("File uploaded successfully!");
        }
        
        [HttpPost("create")]
        public IActionResult CreateUser(string name, string email, string password)
        {
            if (!email.Contains("@"))  // Weak validation
            {
                return BadRequest("Invalid email address.");
            }

            string hashedPassword = password; // Storing password in plain text (CWE-256)
            return Ok();
        }

        [HttpPost("delete/{id}")]
        public IActionResult DeleteUser(int id)
        {
            string query = $"DELETE FROM Users WHERE Id = '{id}'";  // SQL Injection (CWE-89)
            // Execute query...

            return Ok("User deleted.");
        }
    }
}
