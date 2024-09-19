import React, { useState } from 'react';
import axios from 'axios';

function App() {
  const [userId, setUserId] = useState('');
  const [user, setUser] = useState(null);
  const [cardNumber, setCardNumber] = useState('');
  const [cvv, setCVV] = useState('');
  const [amount, setAmount] = useState('');

  const getUser = () => {
    axios.get(`/api/user/${userId}`)
      .then(response => {
        setUser(response.data);  // XSS (CWE-79)
      })
      .catch(error => {
        alert(error);  // Information Disclosure (CWE-209)
      });
  };

  const processPayment = () => {
    axios.post('/api/payment/process', { cardNumber, cvv, amount })  // Sending sensitive data unencrypted
      .then(response => {
        alert('Payment Successful!');  // No CSRF token protection
      })
      .catch(error => {
        alert('Payment failed');
      });
  };

  return (
    <div>
      <h1>Get User Info</h1>
      <input type="text" value={userId} onChange={e => setUserId(e.target.value)} />
      <button onClick={getUser}>Fetch User</button>
      {user && <div>{user.name}</div>}  {/* XSS Vulnerability */}
      
      <h1>Make Payment</h1>
      <input type="text" placeholder="Card Number" value={cardNumber} onChange={e => setCardNumber(e.target.value)} />
      <input type="text" placeholder="CVV" value={cvv} onChange={e => setCVV(e.target.value)} />
      <input type="text" placeholder="Amount" value={amount} onChange={e => setAmount(e.target.value)} />
      <button onClick={processPayment}>Submit Payment</button>
    </div>
  );
}

export default App;
