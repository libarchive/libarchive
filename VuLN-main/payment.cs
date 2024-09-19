using System;
using Microsoft.AspNetCore.Mvc;

namespace VulnerableApp.Controllers
{
    [ApiController]
    [Route("api/[controller]")]
    public class PaymentController : ControllerBase
    {
        private readonly string paymentGatewayUrl = "https://insecure-gateway.com";  // Insecure 3rd-party API (CWE-939)

        [HttpPost("process")]
        public IActionResult ProcessPayment(string cardNumber, string cvv, double amount)
        {
            // Storing credit card info in plaintext (CWE-319)
            var paymentData = new
            {
                CardNumber = cardNumber,  // Sensitive Data Exposure (CWE-200)
                CVV = cvv,
                Amount = amount
            };

            try
            {
                // Send payment request (potential SSRF)
                using (var client = new HttpClient())
                {
                    client.BaseAddress = new Uri(paymentGatewayUrl);
                    var response = client.PostAsJsonAsync("/process", paymentData).Result;  // SSRF Vulnerability (CWE-918)
                    return Ok(response.Content);
                }
            }
            catch (Exception ex)
            {
                return StatusCode(500, "Error processing payment");  // Information disclosure (CWE-209)
            }
        }

        [HttpGet("history")]
        public IActionResult GetPaymentHistory(int userId)
        {
            // No authentication, disclosing payment history to anyone (CWE-284)
            string query = $"SELECT * FROM Payments WHERE UserId = {userId}";  // SQL Injection (CWE-89)
            return Ok("Payment history...");
        }
    }
}
