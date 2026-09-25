import { createBrowserRouter, Navigate, Outlet } from 'react-router-dom';
import { Navbar } from '../components/Navbar/Navbar';
import { DashboardPage } from '../pages/Dashboard/DashboardPage';
import { PurchasePage } from '../pages/Purchase/PurchasePage';
import { QueuePage } from '../pages/Queue/QueuePage';

function RootLayout() {
  return (
    <>
      <Navbar />
      <main className="container">
        <Outlet />
      </main>
    </>
  );
}

export const router = createBrowserRouter([
  {
    path: '/',
    element: <RootLayout />,
    children: [
      { index: true, element: <Navigate to="/dashboard" replace /> },
      { path: 'dashboard', element: <DashboardPage /> },
      { path: 'compra', element: <PurchasePage /> },
      { path: 'fila', element: <QueuePage /> },
      { path: '*', element: <Navigate to="/dashboard" replace /> },
    ],
  },
]);
